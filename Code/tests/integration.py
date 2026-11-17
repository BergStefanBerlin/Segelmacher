"""Isolated native-CLI and Parquet contracts using PyArrow."""
import argparse
import contextlib
import hashlib
import itertools
import json
import math
import os
import pathlib
import re
import signal
import shutil
import subprocess
import tempfile
import threading
import time
import uuid
import pyarrow as pa
import pyarrow.parquet as pq


ROOT = pathlib.Path(__file__).resolve().parents[2]
EXPECTED = "e8c47ffca62459c08ae525320794b19884a72544a2857c77606473ccea862eb2"
BACKUP = ROOT / "Code" / "tests" / "daten" / "main_20261105_195154_358.json"
PARQUET_FILES = ("summen.parquet", "kraefte.parquet", "profile.parquet")


@contextlib.contextmanager
def hold_fingerprint_lock(path):
    if os.name == "nt":
        import ctypes
        from ctypes import wintypes

        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                       wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
        kernel.CreateFileW.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel.CloseHandle.restype = wintypes.BOOL
        handle = kernel.CreateFileW(str(path), 0xC0000000, 0, None, 4, 0x80, None)
        if handle == ctypes.c_void_p(-1).value:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            yield
        finally:
            if not kernel.CloseHandle(handle):
                raise ctypes.WinError(ctypes.get_last_error())
    else:
        import fcntl

        with path.open("a+b") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            try:
                yield
            finally:
                fcntl.flock(lock, fcntl.LOCK_UN)


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parquet_rows(path):
    parquet_file = pq.ParquetFile(path)
    assert parquet_file.metadata.num_row_groups > 0
    for group in range(parquet_file.metadata.num_row_groups):
        metadata = parquet_file.metadata.row_group(group)
        assert all(metadata.column(i).compression == "ZSTD" for i in range(metadata.num_columns))
        assert metadata.num_rows <= 16384
    for batch in parquet_file.iter_batches(batch_size=8192):
        yield from batch.to_pylist()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=pathlib.Path, required=True)
    parser.add_argument("--production-exe", type=pathlib.Path)
    parser.add_argument("--acceptance-exe", type=pathlib.Path)
    parser.add_argument("--parquet-reader", type=pathlib.Path, required=True)
    parser.add_argument("--model-schemas", type=pathlib.Path, required=True)
    parser.add_argument("--publication-test-exe", type=pathlib.Path)
    parser.add_argument("--model-probe", type=pathlib.Path)
    parser.add_argument("--full", action="store_true")
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()
    exe = args.exe.resolve()
    production_exe = (args.production_exe or exe).resolve()
    publication_test_exe = (args.publication_test_exe or exe.with_name("cli_tests" + exe.suffix)).resolve()
    model_probe = (args.model_probe or exe.with_name("model_roundtrip_probe" + exe.suffix)).resolve()
    assert sha(BACKUP) == EXPECTED
    folder = ROOT / "build" / ("integration_" + uuid.uuid4().hex)
    folder.mkdir(parents=True)
    # Never overwrite or clean model folders. Each invocation owns a new directory.
    cases = []

    def fixture(name, budget=2):
        p = folder / (name + "_" + uuid.uuid4().hex)
        p.mkdir()
        shutil.copyfile(BACKUP, p / "main.json")
        shutil.copyfile(ROOT / "Dokumentationen" / "aerodynamik_konventionen.json", p / "konventionen.json")
        job = json.loads((ROOT / "Code" / "ressourcen" / "examples" / "Auftragsdatei.json").read_text(encoding="utf-8"))
        job["ressourcen"]["max_prozessorzahl"] = budget
        if name.startswith("cpu_"):
            job["ressourcen"]["gpu"] = "aus"
        if not args.full:
            job["betriebspunkte"]["anstellwinkel_deg"]["schritt"] = 10
            job["betriebspunkte"]["reynoldszahlen_referenz"] = [450000]
        if not args.strict:
            # Workflow coverage only, not the specified 1% convergence acceptance.
            job["qualitaetspruefung"]["relative_toleranz"] = 0.5
            job["qualitaetspruefung"]["absolute_toleranz_beiwerte"] = 0.5
        (p / "Auftragsdatei.json").write_text(json.dumps(job, indent=2), encoding="utf-8")
        return p, job

    def invoke(p, command="lookup", expected=0, extra=(), cwd=None, env=None):
        command_exe = production_exe if command == "segelmodell" else exe
        cmd = [str(command_exe), command, "--auftrag", str(p / "Auftragsdatei.json"), *extra]
        result = subprocess.run(cmd, cwd=cwd or ROOT, env=env, capture_output=True, encoding="utf-8", timeout=1800)
        log_folder = p if env is None else folder / "fingerprint_logs"
        log_folder.mkdir(exist_ok=True)
        (log_folder / (command + ".stderr.log")).write_text(result.stderr, encoding="utf-8")
        (log_folder / (command + ".stdout.json")).write_text(result.stdout, encoding="utf-8")
        status = json.loads(result.stdout)
        assert status["exitcode"] == result.returncode, (status, result.stderr[-3000:])
        if expected is not None:
            assert status["exitcode"] == expected, (status, result.stderr[-3000:])
        assert not any(c in result.stderr for c in ("\x1b", "\r"))
        cases.append({"name": p.name + ":" + command, "expected": expected, "actual": result.returncode})
        return status, result.stderr

    model_dir, _ = fixture("model_concurrent")
    geometry = json.loads((model_dir / "main.json").read_text(encoding="utf-8"))
    geometry["grundkontur"]["punkte"] = [
        {"id": "hals", "position_m": [0, 0, 0]},
        {"id": "clew", "position_m": [-2, 0, 0]},
        {"id": "aft", "position_m": [-2, 0, 4]},
        {"id": "head", "position_m": [0, 0, 4]},
    ]
    geometry["grundkontur"]["rand"]["kanten"] = [
        {"name": "rand", "punkt_ids": ["hals", "clew", "aft", "head", "hals"]}
    ]
    geometry["flaechenform"]["profilschnitte"]["stationen"] = [
        {"eta": 0, "woelbungsverhaeltnis": 0, "woelbungsposition_relativ": 0.4, "verwindung_deg": 0},
        {"eta": 1, "woelbungsverhaeltnis": 0, "woelbungsposition_relativ": 0.4, "verwindung_deg": 0},
    ]
    (model_dir / "main.json").write_text(json.dumps(geometry), encoding="utf-8")
    model_job = json.loads((ROOT / "Code" / "ressourcen" / "examples" / "Auftrag_Segelmodell.json")
                           .read_text(encoding="utf-8"))
    model_job["ressourcen"]["max_prozessorzahl"] = 2
    model_job["ressourcen"]["gpu"] = "aus"
    (model_dir / "Auftragsdatei.json").write_text(json.dumps(model_job), encoding="utf-8")
    integrity_template = folder / "integrity_template"
    subprocess.run([str(model_probe), str(model_dir / "main.json"),
                    str(model_dir / "Auftragsdatei.json"), str(integrity_template)],
                   check=True, capture_output=True, encoding="utf-8", timeout=600)

    def load_model(path, expected):
        result = subprocess.run([str(publication_test_exe), "--test-load-model", str(path)],
                                capture_output=True, encoding="utf-8", timeout=120)
        assert result.returncode == expected, (path, result.returncode, result.stderr)
        if expected:
            assert result.stderr.strip()

    load_model(integrity_template, 0)
    for name, filename, column, index, value, expected in (
        ("duplicate_strip", "streifen.parquet", "streifen_id", 1, 0, 2),
        ("invalid_chord", "quadratur.parquet", "chord_m", 0, -1.0, 2),
        ("nonfinite_chord", "quadratur.parquet", "chord_m", 0, float("nan"), 3),
        ("missing_reference", "flaechenpunkte.parquet", "streifen_id", 0, 999, 2),
        ("invalid_normal", "flaechenpunkte.parquet", "normal_z", 0, 2.0, 2),
        ("invalid_coupling", "kopplung.parquet", "wert", 0, 999.0, 2),
    ):
        damaged = folder / ("integrity_" + name)
        shutil.copytree(integrity_template, damaged)
        path = damaged / filename
        table = pq.read_table(path)
        manifest_path = damaged / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

        def write_table_and_hash(value):
            pq.write_table(value, path, compression="zstd", use_dictionary=False, row_group_size=16384)
            manifest["tabellen"][filename]["sha256"] = sha(path)
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

        write_table_and_hash(table)
        load_model(damaged, 0)
        field_index = table.schema.get_field_index(column)
        values = table.column(field_index).to_pylist()
        values[index] = value
        table = table.set_column(field_index, table.schema.field(field_index),
                                 pa.array(values, type=table.schema.field(field_index).type))
        write_table_and_hash(table)
        load_model(damaged, expected)
        cases.append({"name": name, "expected": expected, "actual": expected})
    for dirname in ("LookUp", "Segelmodell"):
        (model_dir / dirname).mkdir()
        (model_dir / dirname / "keep.txt").write_text("protected " + dirname, encoding="utf-8")
    protected = {str(path.relative_to(model_dir)): sha(path) for path in (
        model_dir / "main.json", model_dir / "Auftragsdatei.json", model_dir / "konventionen.json",
        model_dir / "LookUp" / "keep.txt")}
    lookup_sentinel_hash = sha(model_dir / "LookUp" / "keep.txt")
    model_sentinel = model_dir / "Segelmodell" / "keep.txt"
    command = [str(production_exe), "segelmodell", "--auftrag", str(model_dir / "Auftragsdatei.json"),
               "--output-policy", "overwrite"]
    legacy = subprocess.run([str(production_exe), "lookup", "--auftrag",
                             str(model_dir / "Auftragsdatei.json")],
                            capture_output=True, encoding="utf-8", timeout=60)
    legacy_status = json.loads(legacy.stdout)
    assert legacy.returncode == 2 and "abgeloest" in legacy_status["meldung"]
    owner = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding="utf-8")
    try:
        deadline = time.monotonic() + 60
        while not (model_dir / "Segelmodell.lock").is_dir():
            assert owner.poll() is None, "Erster Segelmodellprozess vor Sperre beendet"
            assert time.monotonic() < deadline, "Segelmodellsperre nicht erreicht"
            time.sleep(0.05)
        rival = subprocess.run(command, capture_output=True, encoding="utf-8", timeout=60)
        rival_status = json.loads(rival.stdout)
        assert rival.returncode == 5 and rival_status["exitcode"] == 5
        assert "gesperrt" in rival_status["meldung"]
        stdout, stderr = owner.communicate(timeout=600)
        owner_status = json.loads(stdout)
        assert owner.returncode == 0 and owner_status["exitcode"] == 0, (owner_status, stderr[-3000:])
        assert owner_status["output_action"] == "overwritten"
        manifest = json.loads((model_dir / "Segelmodell" / "manifest.json").read_text(encoding="utf-8"))
        evidence = manifest["nachweise"]
        assert set(evidence["nicht_ausgefuehrt"]) == {
            "vollstaendige_numerikabnahme", "datenformat_und_integritaet",
            "end_to_end_integration", "geschuetzte_publikation"}
        assert not evidence["fehlgeschlagen"] and evidence["physikalisch_validiert"] is False
        assert not (model_dir / "Segelmodell.lock").exists()
        assert not list(model_dir.glob("Segelmodell.arbeiten.*"))
        assert protected == {name: sha(model_dir / name) for name in protected}
        assert sha(model_dir / "LookUp" / "keep.txt") == lookup_sentinel_hash
        assert not model_sentinel.exists()
        load_model(model_dir / "Segelmodell", 0)
        cases.append({"name": "model_concurrent", "expected": [0, 5],
                      "actual": [owner.returncode, rival.returncode]})
    finally:
        if owner.poll() is None:
            owner.kill()
            owner.communicate()

    if os.name == "nt":
        candidate = (args.acceptance_exe or exe.with_name("SegelMacherAbnahme" + exe.suffix)).resolve()
        outside = subprocess.run([str(candidate), "segelmodell", "--auftrag",
                                  str(model_dir / "Auftragsdatei.json"), "--output-policy", "overwrite"],
                                 capture_output=True, encoding="utf-8", timeout=60)
        outside_status = json.loads(outside.stdout)
        assert outside.returncode == 5 and "temporaeren Verzeichnisses" in outside_status["meldung"]
        legacy = subprocess.run([str(candidate), "lookup", "--auftrag",
                                 str(model_dir / "Auftragsdatei.json")],
                                capture_output=True, encoding="utf-8", timeout=60)
        assert legacy.returncode == 2 and "nicht verfuegbar" in json.loads(legacy.stdout)["meldung"]
        assert protected == {name: sha(model_dir / name) for name in protected}

        with tempfile.TemporaryDirectory(prefix="segelmacher-abnahme-") as temp:
            model = pathlib.Path(temp) / "testmodel"
            aero = model / "modells" / "aerodynamics" / "main"
            aero.mkdir(parents=True)
            for name in ("main.json", "Auftragsdatei.json", "konventionen.json"):
                shutil.copyfile(model_dir / name, aero / name)
            (aero / "LookUp").mkdir()
            (aero / "LookUp" / "keep.txt").write_text("unaltered", encoding="utf-8")
            input_paths = (aero / "main.json", aero / "Auftragsdatei.json",
                           aero / "konventionen.json", aero / "LookUp" / "keep.txt")
            input_hashes = {path: sha(path) for path in input_paths}
            identity = {"id": "testmodel", "name": "Fingerprint test", "type": "body"}
            schema = {"schema_version": "0.3", "release": False}
            (model / "modells_data.json").write_text(json.dumps({
                "schema": schema, "identity": identity, "groups": {"body": {"elements": [{
                    "id": "body", "display": {"hidden": True},
                    "placement": {"translation_m": [0, 0, 0], "rotation_xyzw": [0, 0, 0, 1]}
                }]}}
            }), encoding="utf-8")
            fingerprints = {key: {"hash": "0" * 64, "modifiedAt": "2026-01-01"}
                            for key in ("visualisation", "mass", "hydrostatics", "aerodynamics", "modelData")}
            validation = model / "modells_validation.json"
            validation.write_text(json.dumps({"schema": schema, "identity": identity,
                                              "model": {"fingerprints": fingerprints, "origins": []}}),
                                  encoding="utf-8")
            env = os.environ.copy()
            env["SEGELPHYSIK_TEST_SCHEMA_ROOT"] = str(args.model_schemas.resolve())
            run = subprocess.run([str(candidate), "segelmodell", "--auftrag", str(aero / "Auftragsdatei.json"),
                                  "--fingerprint", "update"], env=env, capture_output=True,
                                 encoding="utf-8", timeout=600)
            status = json.loads(run.stdout)
            assert run.returncode == 0 and status["exitcode"] == 0, (status, run.stderr[-3000:])
            assert status["modus"] == "isolierte_abnahme" and status["projektfreigabe"] is False
            assert status["output_action"] == "created"
            output = aero / "Segelmodell"
            load_model(output, 0)
            manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
            assert manifest["erzeuger"]["werkzeug"].endswith("-isolierte-abnahme")
            evidence = manifest["nachweise"]
            assert set(evidence["nicht_ausgefuehrt"]) == {
                "vollstaendige_numerikabnahme", "datenformat_und_integritaet",
                "end_to_end_integration", "geschuetzte_publikation"}
            assert not evidence["fehlgeschlagen"] and evidence["physikalisch_validiert"] is False
            assert set(evidence["bestanden"]) == {
                "rastervergleich_vorpruefung", "nullwindfolge", "gleichwind_vorpruefung",
                "h1_referenzvergleich_vorpruefung", "tabellen_roundtrip_vorpruefung",
                "modell_lade_und_rechenprobe"}
            for name, metadata in manifest["tabellen"].items():
                path = output / name
                assert sha(path) == metadata["sha256"].lower()
                assert sum(1 for _ in parquet_rows(path)) == metadata["zeilen"]
            after = json.loads(validation.read_text(encoding="utf-8"))
            assert after["model"]["fingerprints"]["aerodynamics"]["hash"] == status["fingerprint"]["hash"]
            for key in fingerprints:
                if key != "aerodynamics":
                    assert after["model"]["fingerprints"][key] == fingerprints[key]
            prior = pathlib.Path(temp) / "vorpruefbericht.json"
            prior.write_text(json.dumps(owner_status), encoding="utf-8")
            original_manifest_hash = sha(output / "manifest.json")
            reuse_args = [str(candidate), "segelmodell", "--auftrag", str(aero / "Auftragsdatei.json"),
                          "--output-policy", "overwrite", "--vorpruefbericht", str(prior),
                          "--vorpruefbericht-sha256"]
            rejected = subprocess.run([*reuse_args, "0" * 64], env=env, capture_output=True,
                                      encoding="utf-8", timeout=60)
            assert rejected.returncode == 4 and "SHA-256 stimmt nicht" in json.loads(rejected.stdout)["meldung"]
            assert sha(output / "manifest.json") == original_manifest_hash
            altered = json.loads(prior.read_text(encoding="utf-8"))
            altered["werkzeugversion"] = "abweichend"
            altered_path = pathlib.Path(temp) / "abweichender-vorpruefbericht.json"
            altered_path.write_text(json.dumps(altered), encoding="utf-8")
            mismatch_args = [str(candidate), "segelmodell", "--auftrag", str(aero / "Auftragsdatei.json"),
                             "--output-policy", "overwrite", "--vorpruefbericht", str(altered_path),
                             "--vorpruefbericht-sha256", sha(altered_path)]
            mismatch = subprocess.run(mismatch_args, env=env, capture_output=True, encoding="utf-8", timeout=60)
            assert mismatch.returncode == 4 and "Werkzeugversion" in json.loads(mismatch.stdout)["meldung"]
            assert sha(output / "manifest.json") == original_manifest_hash
            altered = json.loads(prior.read_text(encoding="utf-8"))
            altered["eingaben"]["geometrie"]["sha256"] = "0" * 64
            altered_path.write_text(json.dumps(altered), encoding="utf-8")
            mismatch_args[-1] = sha(altered_path)
            mismatch = subprocess.run(mismatch_args, env=env, capture_output=True, encoding="utf-8", timeout=60)
            assert mismatch.returncode == 4 and "passt nicht zur Eingabe" in json.loads(mismatch.stdout)["meldung"], (
                mismatch.returncode, mismatch.stdout, mismatch.stderr)
            assert sha(output / "manifest.json") == original_manifest_hash
            altered = json.loads(prior.read_text(encoding="utf-8"))
            altered["numerik_vorpruefung"]["tabellen_roundtrip"]["tabellen"]["streifen.parquet"]["sha256"] = "0" * 64
            altered_path.write_text(json.dumps(altered), encoding="utf-8")
            mismatch_args[-1] = sha(altered_path)
            mismatch = subprocess.run(mismatch_args, env=env, capture_output=True, encoding="utf-8", timeout=120)
            assert mismatch.returncode == 4 and "passt nicht zu den erzeugten Tabellen" in json.loads(mismatch.stdout)["meldung"]
            assert sha(output / "manifest.json") == original_manifest_hash
            reused = subprocess.run([*reuse_args, sha(prior), "--fingerprint", "update"], env=env,
                                    capture_output=True, encoding="utf-8", timeout=600)
            reused_status = json.loads(reused.stdout)
            assert reused.returncode == 0, (reused_status, reused.stderr[-3000:])
            assert reused_status["output_action"] == "overwritten"
            assert reused_status["numerikquelle"] == "uebernommener_vorpruefbericht"
            assert reused_status["projektfreigabe"] is False
            reused_manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
            assert reused_manifest["eingaben"]["vorpruefbericht"]["sha256"] == sha(prior)
            assert reused_manifest["eingaben"]["vorpruefbericht"]["schema_version"] == "unversioniert"
            assert reused_manifest["tabellen"] == manifest["tabellen"]
            assert reused_manifest["nachweise"] == manifest["nachweise"]
            after = json.loads(validation.read_text(encoding="utf-8"))
            assert after["model"]["fingerprints"]["aerodynamics"]["hash"] == reused_status["fingerprint"]["hash"]
            for key in fingerprints:
                if key != "aerodynamics":
                    assert after["model"]["fingerprints"][key] == fingerprints[key]
            assert input_hashes == {path: sha(path) for path in input_paths}
            assert not (aero / "Segelmodell.lock").exists()
            assert not list(aero.glob("Segelmodell.arbeiten.*"))
            protected_model = {path.name: sha(path) for path in output.iterdir() if path.is_file()}
            child = subprocess.Popen(
                [str(candidate), "segelmodell", "--auftrag", str(aero / "Auftragsdatei.json"),
                 "--output-policy", "overwrite"],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding="utf-8", env=env,
                creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
            started = threading.Event()
            stderr_lines = []

            def capture_model_stderr():
                for line in child.stderr:
                    stderr_lines.append(line)
                    if "Beginn: Segelmodell-Rastergradienten" in line:
                        started.set()

            reader = threading.Thread(target=capture_model_stderr, daemon=True)
            reader.start()
            try:
                assert started.wait(60), "Segelmodellprozess erreichte Rasterpruefung nicht"
                child.send_signal(signal.CTRL_BREAK_EVENT)
                child.wait(timeout=180)
                stdout = child.stdout.read()
                reader.join(timeout=10)
                cancelled_status = json.loads(stdout)
                assert child.returncode == cancelled_status["exitcode"] == 130, (
                    cancelled_status, "".join(stderr_lines))
                assert protected_model == {path.name: sha(path) for path in output.iterdir() if path.is_file()}
                assert input_hashes == {path: sha(path) for path in input_paths}
                assert not (aero / "Segelmodell.lock").exists()
                assert not list(aero.glob("Segelmodell.arbeiten.*"))
                cases.append({"name": "in_flight_segelmodell_cancel", "expected": 130,
                              "actual": child.returncode})
            finally:
                if child.poll() is None:
                    child.terminate()
                    child.wait(timeout=10)
                reader.join(timeout=10)
            cases.append({"name": "isolated_model_publication", "expected": 0, "actual": run.returncode,
                          "manifest_sha256": sha(output / "manifest.json"),
                          "table_sha256": {name: item["sha256"] for name, item in manifest["tabellen"].items()},
                          "fingerprint": reused_status["fingerprint"]["hash"], "project_release": False,
                          "vorpruefbericht_sha256": sha(prior), "numerikquelle": reused_status["numerikquelle"]})

    p, job = fixture("420 räumlich")
    invoke(p, "pruefen", cwd=folder)
    invoke(p, "pruefen", cwd=p)
    _, log = invoke(p)
    output = p / "LookUp"
    assert set(f.name for f in output.iterdir()) == {
        "lookup.json", *PARQUET_FILES, "pruefbericht.json", "auftrag_ausgefuehrt.json"
    }
    meta = json.loads((output / "lookup.json").read_text(encoding="utf-8"))
    assert meta["formatversion"] == "3"
    assert meta["windkonvention"]["neigungsbereich_deg"] == [-90, 90]
    assert meta["windkonvention"]["anliegende_ueberblendung_abs_beta_deg"] == [60, 80]
    assert meta["reynolds_referenz"]["bezugspunkt"] == "segelhals"
    assert meta["reynolds_referenz"]["interpolation"] == \
        "linear_in_log_Re_innerhalb_manifestierter_stuetzstellen"
    assert meta["qualitaet"] == "ingenieurmodell_nicht_validiert"
    assert meta["parquet"]["bibliothek"] == "Apache Arrow"
    assert meta["parquet"]["bibliotheksversion"] == "21.0.0"
    assert meta["parquet"]["schema_version"] == "1"
    assert meta["parquet"]["kompression"] == "ZSTD"
    assert meta["parquet"]["row_group_max_zeilen"] == 16384
    assert set(meta["parquet"]["tabellen"]) == set(PARQUET_FILES)
    assert set(meta["dateien_sha256"]) == {*PARQUET_FILES, "pruefbericht.json", "auftrag_ausgefuehrt.json"}
    assert not any(path.suffix.lower() == ".csv" for path in output.iterdir())
    expected_schemas = {
        "summen.parquet": [
            ("form_id", pa.string(), False), ("alpha_deg", pa.float64(), False),
            ("windneigung_deg", pa.float64(), False), ("re_ref", pa.float64(), False),
            ("CFx", pa.float64(), False), ("CFy", pa.float64(), False), ("CFz", pa.float64(), False),
            ("CMx", pa.float64(), False), ("CMy", pa.float64(), False), ("CMz", pa.float64(), False),
            ("angriff_x_m", pa.float64(), True), ("angriff_y_m", pa.float64(), True),
            ("angriff_z_m", pa.float64(), True), ("angriff_gueltig", pa.bool_(), False),
            ("CM_rest_x", pa.float64(), False), ("CM_rest_y", pa.float64(), False),
            ("CM_rest_z", pa.float64(), False), ("qualitaet", pa.string(), False),
        ],
        "kraefte.parquet": [
            ("form_id", pa.string(), False), ("alpha_deg", pa.float64(), False),
            ("windneigung_deg", pa.float64(), False), ("re_ref", pa.float64(), False),
            ("beitrag_id", pa.int64(), False), ("CFx", pa.float64(), False),
            ("CFy", pa.float64(), False), ("CFz", pa.float64(), False),
            ("ansatz_x_m", pa.float64(), False), ("ansatz_y_m", pa.float64(), False),
            ("ansatz_z_m", pa.float64(), False), ("CM_eigen_x", pa.float64(), False),
            ("CM_eigen_y", pa.float64(), False), ("CM_eigen_z", pa.float64(), False),
            ("qualitaet", pa.string(), False),
        ],
        "profile.parquet": [
            ("profil_id", pa.string(), False), ("alpha_deg", pa.float64(), False),
            ("re", pa.float64(), False), ("CL", pa.float64(), False),
            ("CD", pa.float64(), False), ("CM", pa.float64(), False),
            ("qualitaet", pa.string(), False),
        ],
    }
    for filename, fields in expected_schemas.items():
        parquet_file = pq.ParquetFile(output / filename)
        schema = parquet_file.schema_arrow
        expected_schema = pa.schema([pa.field(name, dtype, nullable=nullable)
                                     for name, dtype, nullable in fields])
        assert schema.equals(expected_schema), (filename, schema, expected_schema)
        table_metadata = meta["parquet"]["tabellen"][filename]
        assert parquet_file.metadata.num_rows == table_metadata["zeilen"]
        assert parquet_file.metadata.num_row_groups == table_metadata["row_groups"]
        parquet_file.close()
    reader = subprocess.run(
        [str(args.parquet_reader.resolve()), str(output)],
        capture_output=True, encoding="utf-8", timeout=60
    )
    assert reader.returncode == 0, reader.stderr
    support = meta["interpolation"]["stuetzstellen"]
    support_angles = support["ausgegeben_azimut_deg"]
    support_elevations = support["ausgegeben_windneigung_deg"]
    assert support["basis_windneigung_deg"] == list(range(-90, 91, 10))
    assert len(support_elevations) == 45
    assert min(support_elevations) == -90 and max(support_elevations) == 90
    assert support_elevations == sorted(support_elevations)
    assert all(-beta in support_elevations for beta in support_elevations)
    assert {-88.75, 88.75}.issubset(support_elevations)
    assert set(support["basis_azimut_deg"]).issubset(support_angles)
    assert set(support["basis_windneigung_deg"]).issubset(support_elevations)
    report = json.loads((output / "pruefbericht.json").read_text(encoding="utf-8"))
    for section in ("numerik", "wirbel_provider"):
        if section not in report:
            continue
        assert report[section]["absolute_toleranz"] == job["qualitaetspruefung"]["absolute_toleranz_beiwerte"]
        assert report[section]["relative_toleranz"] == job["qualitaetspruefung"]["relative_toleranz"]
        for quantity in ("kraft", "moment"):
            metrics = report[section]["prozentkennzahlen"][quantity]
            assert metrics["vergleiche"] == report[section]["vergleiche"]
            for numerator, denominator, percentage in (
                ("rms_abweichung", "rms_referenz", "rms_relativ_prozent"),
                ("max_abweichung", "max_referenz", "max_abweichung_zu_spitzenlast_prozent"),
            ):
                if metrics[denominator] == 0:
                    assert metrics[percentage] is None
                else:
                    assert math.isclose(metrics[percentage], 100 * metrics[numerator] / metrics[denominator], rel_tol=1e-12, abs_tol=1e-12)
    gpu = report["gpu"]
    assert gpu["speedbenchmark"] is False
    if gpu["backend"].startswith("OpenCL"):
        assert gpu["plattform"] and gpu["geraet"]
        assert gpu["ausgefuehrte_polarwerte"] > 0
        assert gpu["cpu_gpu_vergleiche"] == 3 * gpu["ausgefuehrte_polarwerte"]
        assert gpu["max_cpu_gpu_abweichung"] <= 1e-10
        if gpu["backend"] == "OpenCL":
            assert gpu["ausgefuehrte_polarwerte"] > 0
        else:
            assert "CPU-Fallback" in gpu["backend"]
    else:
        assert gpu["cpu_gpu_vergleiche"] == 0
        assert gpu["auswahl"] in ("cpu", "cpu_fallback")
    if args.strict:
        assert report["numerik"]["fehler"] == 0
        assert report["wirbel_provider"]["fehler"] == 0
        assert meta["interpolation"]["verfahren"] == "periodische_tensorprodukt_cubic_hermite"
        assert set(support["basis_azimut_deg"]).issubset(support_angles)
        assert set(support["basis_windneigung_deg"]).issubset(support_elevations)
        assert len(support_elevations) > len(support["basis_windneigung_deg"])
        assert set(range(-90, 91, 10)).issubset(set(support["basis_windneigung_deg"]))
    for name, digest in meta["dateien_sha256"].items():
        assert sha(output / name) == digest
    sums = {}
    previous = None
    poles = {}
    for row in parquet_rows(output / "summen.parquet"):
        key = (row["form_id"], row["alpha_deg"], row["windneigung_deg"], row["re_ref"])
        assert key not in sums
        assert previous is None or key > previous
        previous = key
        f = [row[k] for k in ("CFx", "CFy", "CFz")]
        m = [row[k] for k in ("CMx", "CMy", "CMz")]
        assert all(math.isfinite(x) for x in f + m)
        if abs(key[2]) == 90:
            polekey = key[0], key[3], key[2]
            assert polekey not in poles or poles[polekey] == f + m
            poles[polekey] = f + m
        sums[key] = (f, m)
        if row["angriff_gueltig"]:
            r = [row[k] / meta["bezuege"]["c_ref_m"] for k in ("angriff_x_m", "angriff_y_m", "angriff_z_m")]
            rest = [row[k] for k in ("CM_rest_x", "CM_rest_y", "CM_rest_z")]
            cross = [r[1]*f[2]-r[2]*f[1], r[2]*f[0]-r[0]*f[2], r[0]*f[1]-r[1]*f[0]]
            assert max(abs(m[i]-cross[i]-rest[i]) for i in range(3)) < 1e-10
        else:
            assert all(row[k] is None for k in ("angriff_x_m", "angriff_y_m", "angriff_z_m"))
    expected_rows = len(support_angles) * len(support_elevations) * len(job["betriebspunkte"]["reynoldszahlen_referenz"])
    if args.full:
        assert expected_rows >= 21600
    assert len(support_elevations) >= 10
    assert len(sums) == expected_rows, len(sums)
    assert any(key[2] < 0 for key in sums)
    integrated = {}
    previous_force_key = None
    for row in parquet_rows(output / "kraefte.parquet"):
        key = row["form_id"], row["alpha_deg"], row["windneigung_deg"], row["re_ref"]
        force_key = (*key, row["beitrag_id"])
        assert previous_force_key is None or force_key > previous_force_key
        previous_force_key = force_key
        f = [row[k] for k in ("CFx", "CFy", "CFz")]
        r = [row[k] / meta["bezuege"]["c_ref_m"] for k in ("ansatz_x_m", "ansatz_y_m", "ansatz_z_m")]
        m = [row[k] for k in ("CM_eigen_x", "CM_eigen_y", "CM_eigen_z")]
        assert all(math.isfinite(x) for x in f + r + m)
        value = integrated.setdefault(key, [0.0]*6)
        moment = [r[1]*f[2]-r[2]*f[1]+m[0], r[2]*f[0]-r[0]*f[2]+m[1], r[0]*f[1]-r[1]*f[0]+m[2]]
        for i, x in enumerate(f + moment):
            value[i] += x
    for key, (f, m) in sums.items():
        assert max(abs(integrated[key][i]-(f+m)[i]) for i in range(6)) < 1e-10
    assert "Gesamt: 100 %" in log and "Phasen: 6/6" in log
    progress = [int(x) for x in re.findall(r"Gesamt: (\d+) %", log)]
    assert progress == sorted(progress)
    previous = None
    profile_ids = {row["profil_id"] for row in meta["geometrie"]["grids"]["4"]}
    for row in parquet_rows(output / "profile.parquet"):
        key = row["profil_id"], row["alpha_deg"], row["re"]
        assert key[0] in profile_ids
        assert previous is None or key > previous
        previous = key
        assert all(math.isfinite(row[k]) for k in ("CL", "CD", "CM"))
    before = {name: sha(output / name) for name in meta["dateien_sha256"]}
    skipped, _ = invoke(p)
    assert skipped["output_action"] == "skipped"
    assert before == {name: sha(output / name) for name in before}
    if not args.full:
        (output / "obsolete.txt").write_text("old output", encoding="utf-8")
        replaced, _ = invoke(p, extra=("--output-policy", "overwrite"))
        assert replaced["output_action"] == "overwritten"
        assert not (output / "obsolete.txt").exists()
        assert not list(p.glob("LookUp.ersetzen.*"))
        assert {name: before[name] for name in PARQUET_FILES} == \
               {name: sha(output / name) for name in PARQUET_FILES}
        model = folder / "testmodel"
        aero = model / "modells" / "aerodynamics" / "main"
        aero.mkdir(parents=True)
        shutil.copytree(output, aero / "LookUp")
        for name in ("main.json", "konventionen.json", "Auftragsdatei.json"):
            shutil.copyfile(p / name, aero / name)
        identity = {"id": "testmodel", "name": "Fingerprint test", "type": "body"}
        schema = {"schema_version": "0.3", "release": False}
        (model / "modells_data.json").write_text(json.dumps({
            "schema": schema, "identity": identity, "groups": {"body": {"elements": [{
                "id": "body", "display": {"hidden": True},
                "placement": {"translation_m": [0, 0, 0], "rotation_xyzw": [0, 0, 0, 1]}
            }]}}
        }), encoding="utf-8")
        fingerprints = {key: {"hash": "0" * 64, "modifiedAt": "2026-01-01"}
                        for key in ("visualisation", "mass", "hydrostatics", "aerodynamics", "modelData")}
        validation = {"schema": schema, "identity": identity,
                      "model": {"fingerprints": fingerprints, "origins": []}}
        validation_file = model / "modells_validation.json"
        validation_file.write_text(json.dumps(validation), encoding="utf-8")
        env = os.environ.copy()
        env["SEGELPHYSIK_TEST_SCHEMA_ROOT"] = str(args.model_schemas.resolve())
        original_validation = validation_file.read_bytes()
        invoke(aero, extra=("--fingerprint", "skip"), env=env)
        assert validation_file.read_bytes() == original_validation
        updated, _ = invoke(aero, extra=("--fingerprint", "update"), env=env)
        assert updated["output_action"] == "skipped"
        assert updated["fingerprint"]["updated"] is True
        after = json.loads(validation_file.read_text(encoding="utf-8"))
        assert after["model"]["fingerprints"]["aerodynamics"]["hash"] == updated["fingerprint"]["hash"]
        for key in fingerprints:
            if key != "aerodynamics":
                assert after["model"]["fingerprints"][key] == fingerprints[key]
        validation_bytes = validation_file.read_bytes()
        repeated, _ = invoke(aero, extra=("--fingerprint", "update"), env=env)
        assert repeated["fingerprint"]["updated"] is False
        assert validation_file.read_bytes() == validation_bytes
        (model / "modells" / "aerodynamics" / "hidden.txt").write_text("new resource", encoding="utf-8")
        changed, _ = invoke(aero, extra=("--fingerprint", "update"), env=env)
        assert changed["fingerprint"]["hash"] != updated["fingerprint"]["hash"]
        validation_bytes = validation_file.read_bytes()
        with hold_fingerprint_lock(model / ".modells_validation.lock"):
            failed, _ = invoke(aero, extra=("--fingerprint", "update"), env=env, expected=5)
            assert "CORE-FPR-0002" in failed["meldung"]
            assert validation_file.read_bytes() == validation_bytes
            failed_after_write, _ = invoke(
                aero, extra=("--output-policy", "overwrite", "--fingerprint", "update"),
                env=env, expected=5)
            assert failed_after_write["output_action"] == "written"
            assert pathlib.Path(failed_after_write["ergebnisordner"]) == aero / "LookUp"
            assert validation_file.read_bytes() == validation_bytes
        written, _ = invoke(aero, extra=("--output-policy", "overwrite", "--fingerprint", "update"), env=env)
        assert written["output_action"] == "overwritten"
        stable, _ = invoke(aero, extra=("--fingerprint", "update"), env=env)
        assert stable["fingerprint"]["updated"] is False
        assert stable["fingerprint"]["hash"] == written["fingerprint"]["hash"]
        model_job = json.loads((ROOT / "Code" / "ressourcen" / "examples" / "Auftrag_Segelmodell.json")
                               .read_text(encoding="utf-8"))
        (aero / "Auftragsdatei.json").write_text(json.dumps(model_job, indent=2), encoding="utf-8")
        existing_model = aero / "Segelmodell"
        existing_model.mkdir()
        sentinel = existing_model / "keep.txt"
        sentinel.write_text("unchanged model", encoding="utf-8")
        sentinel_hash = sha(sentinel)
        lookup_hashes = {path.name: sha(path) for path in (aero / "LookUp").iterdir() if path.is_file()}
        validation_bytes = validation_file.read_bytes()
        model_skipped, _ = invoke(aero, command="segelmodell", extra=("--fingerprint", "skip"), env=env)
        assert model_skipped["output_action"] == "skipped"
        assert validation_file.read_bytes() == validation_bytes and sha(sentinel) == sentinel_hash
        model_updated, _ = invoke(aero, command="segelmodell", extra=("--fingerprint", "update"), env=env)
        assert model_updated["output_action"] == "skipped"
        assert model_updated["fingerprint"]["bereich"] == "aerodynamics"
        after = json.loads(validation_file.read_text(encoding="utf-8"))
        assert after["model"]["fingerprints"]["aerodynamics"]["hash"] == model_updated["fingerprint"]["hash"]
        for key in fingerprints:
            if key != "aerodynamics":
                assert after["model"]["fingerprints"][key] == fingerprints[key]
        validation_bytes = validation_file.read_bytes()
        with hold_fingerprint_lock(model / ".modells_validation.lock"):
            failed, _ = invoke(aero, command="segelmodell", extra=("--fingerprint", "update"), env=env, expected=5)
            assert "CORE-FPR-0002" in failed["meldung"]
        assert validation_file.read_bytes() == validation_bytes and sha(sentinel) == sentinel_hash
        assert lookup_hashes == {path.name: sha(path) for path in (aero / "LookUp").iterdir() if path.is_file()}
        assert not (aero / "Segelmodell.lock").exists()
        assert not list(aero.glob("Segelmodell.arbeiten.*"))
        if os.name == "nt":
            # Test-only simulated evidence; this is not a productive acceptance.
            template = folder / "publication_test_template"
            subprocess.run([str(model_probe), str(model_dir / "main.json"),
                            str(model_dir / "Auftragsdatei.json"), str(template)],
                           check=True, capture_output=True, encoding="utf-8", timeout=600)
            manifest_path = template / "manifest.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            conventions = model_dir / "konventionen.json"
            manifest["eingaben"]["konventionen"] = {
                "pfad": str(conventions), "sha256": sha(conventions),
                "schema_version": json.loads(conventions.read_text(encoding="utf-8"))["schema_version"]}
            required = ["rastervergleich_vorpruefung", "nullwindfolge", "gleichwind_vorpruefung",
                        "h1_referenzvergleich_vorpruefung", "tabellen_roundtrip_vorpruefung",
                        "modell_lade_und_rechenprobe"]
            manifest["nachweise"].update(ausgefuehrt=required, bestanden=required,
                                         fehlgeschlagen=[], nicht_ausgefuehrt=[])
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

            def publish_test(token):
                stage = aero / ("Segelmodell.arbeiten." + token)
                shutil.copytree(template, stage)
                proc = subprocess.run([str(publication_test_exe),
                                       "--test-publish-fingerprint", str(stage),
                                       str(existing_model), str(model)],
                                      env=env, capture_output=True, encoding="utf-8", timeout=600)
                (folder / ("publication_" + token + ".stdout.json")).write_text(proc.stdout, encoding="utf-8")
                (folder / ("publication_" + token + ".stderr.log")).write_text(proc.stderr, encoding="utf-8")
                result = json.loads(proc.stdout)
                assert result["exitcode"] == proc.returncode, (result, proc.stderr)
                assert not stage.exists()
                assert not (aero / ("Segelmodell.ersetzen." + token)).exists()
                return result

            published = publish_test("simulated_success")
            assert published["status"] == "erfolgreich" and published["output_action"] == "overwritten", published
            assert pathlib.Path(published["ergebnisordner"]) == existing_model
            assert not sentinel.exists()
            after = json.loads(validation_file.read_text(encoding="utf-8"))
            assert after["model"]["fingerprints"]["aerodynamics"]["hash"] == published["fingerprint"]["hash"]
            for key in fingerprints:
                if key != "aerodynamics":
                    assert after["model"]["fingerprints"][key] == fingerprints[key]
            validation_bytes = validation_file.read_bytes()
            with hold_fingerprint_lock(model / ".modells_validation.lock"):
                failed = publish_test("simulated_locked")
                assert failed["status"] == "fehlgeschlagen" and failed["exitcode"] == 5, failed
                assert failed["output_action"] == "written"
                assert pathlib.Path(failed["ergebnisordner"]) == existing_model
                assert "CORE-FPR-0002" in failed["meldung"]
                assert validation_file.read_bytes() == validation_bytes
            assert (existing_model / "manifest.json").is_file()
            assert lookup_hashes == {path.name: sha(path) for path in (aero / "LookUp").iterdir() if path.is_file()}
            cases.append({"name": "publication_fingerprint_simulated_evidence",
                          "expected": [0, 5], "actual": [published["exitcode"], failed["exitcode"]]})
    assert json.loads((output / "auftrag_ausgefuehrt.json").read_text(encoding="utf-8"))["geometrie"]
    if not args.full:
        cpu_reference_hashes = None
        for budget in (1, 4):
            limited, _ = fixture("cpu_" + str(budget), budget)
            invoke(limited)
            hashes = {name: sha(limited / "LookUp" / name) for name in PARQUET_FILES}
            if cpu_reference_hashes is None:
                cpu_reference_hashes = hashes
            else:
                assert hashes == cpu_reference_hashes
            report = json.loads((limited / "LookUp" / "pruefbericht.json").read_text(encoding="utf-8"))
            assert report["gpu"]["backend"] == "CPU"
            assert report["cpu_effektiv"] <= budget
            assert 1 <= report["cpu_peak"] <= report["cpu_effektiv"]
        if gpu["backend"] == "OpenCL":
            gpu_rows = parquet_rows(output / "profile.parquet")
            cpu_rows = parquet_rows(limited / "LookUp" / "profile.parquet")
            for gpu_row, cpu_row in itertools.zip_longest(gpu_rows, cpu_rows):
                assert gpu_row is not None and cpu_row is not None
                assert (gpu_row["profil_id"], gpu_row["alpha_deg"], gpu_row["re"]) == \
                       (cpu_row["profil_id"], cpu_row["alpha_deg"], cpu_row["re"])
                for key in ("CL", "CD", "CM"):
                    g, c = gpu_row[key], cpu_row[key]
                    assert abs(g-c) <= 2e-12+2e-12*abs(c)
    if not args.full:
        for name, mutate, code in (
            ("draft", lambda j: j.update(status="entwurf_rechenparameter_nicht_freigegeben"), 2),
            ("unknown", lambda j: j["berechnung"].update(unbekannt=True), 2),
            ("bad_re", lambda j: j["betriebspunkte"].update(reynoldszahlen_referenz=[1000]), 3),
            ("bad_version", lambda j: j.update(schema_version="99"), 2),
        ):
            target, spec = fixture(name)
            mutate(spec)
            (target / "Auftragsdatei.json").write_text(json.dumps(spec), encoding="utf-8")
            invoke(target, expected=code)
            assert not (target / "LookUp").exists()
        target, legacy_job = fixture("legacy_axis")
        legacy_job["betriebspunkte"]["windneigung_deg"] = list(range(0, 91, 10))
        (target / "Auftragsdatei.json").write_text(json.dumps(legacy_job), encoding="utf-8")
        legacy_status, _ = invoke(target, "pruefen", expected=2)
        assert "alte Auftraege" in legacy_status["meldung"]
        assert not (target / "LookUp").exists()
        target, spec = fixture("gpu_required")
        spec["ressourcen"]["gpu"] = "erforderlich"
        (target / "Auftragsdatei.json").write_text(json.dumps(spec), encoding="utf-8")
        status, _ = invoke(target, "pruefen", expected=None)
        assert status["exitcode"] in (0, 3)
        if status["exitcode"] == 3:
            assert "GPU erforderlich:" in status["meldung"]
        target, _ = fixture("locked")
        (target / "LookUp.lock").mkdir()
        invoke(target, expected=5)
        assert (target / "LookUp.lock").exists()
        target, cancel_job = fixture("cancel", 1)
        reference_job = json.loads((ROOT / "Code" / "ressourcen" / "examples" / "Auftragsdatei.json").read_text(encoding="utf-8"))
        cancel_job["betriebspunkte"] = reference_job["betriebspunkte"]
        (target / "Auftragsdatei.json").write_text(json.dumps(cancel_job), encoding="utf-8")
        command = [str(exe), "lookup", "--auftrag", str(target / "Auftragsdatei.json")]
        child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding="utf-8",
                                 creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
        stderr_lines = []
        in_flight = threading.Event()
        def capture_stderr():
            for line in child.stderr:
                stderr_lines.append(line)
                if "[Gesamtsegelintegration]" in line and "Teil:" in line:
                    in_flight.set()
        reader = threading.Thread(target=capture_stderr, daemon=True)
        reader.start()
        if not in_flight.wait(60):
            child.terminate()
            child.wait(timeout=10)
            reader.join(timeout=10)
            raise AssertionError("integration process never reached active worker execution")
        # Windows disables Ctrl+C in CREATE_NEW_PROCESS_GROUP children;
        # Ctrl+Break can target only this owned process group.
        child.send_signal(signal.CTRL_BREAK_EVENT)
        try:
            child.wait(timeout=180)
        except subprocess.TimeoutExpired:
            child.terminate()
            child.wait(timeout=10)
            reader.join(timeout=10)
            raise AssertionError("integration process did not stop after Ctrl+Break")
        stdout = child.stdout.read()
        reader.join(timeout=10)
        (target / "cancel.stderr.log").write_text("".join(stderr_lines), encoding="utf-8")
        cancel_status = json.loads(stdout)
        assert cancel_status["exitcode"] == child.returncode == 130, cancel_status
        assert not (target / "LookUp").exists()
        assert not (target / "LookUp.lock").exists()
        assert cancel_status["pruefbericht"] and pathlib.Path(cancel_status["pruefbericht"]).exists()
        cases.append({"name": "in_flight_cancel", "expected": 130, "actual": child.returncode})
        target, _ = fixture("concurrent")
        command = [str(exe), "lookup", "--auftrag", str(target / "Auftragsdatei.json")]
        children = [subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding="utf-8") for _ in range(2)]
        results = []
        for index, child in enumerate(children):
            stdout, stderr = child.communicate(timeout=180)
            (target / f"concurrent_{index}.stderr.log").write_text(stderr, encoding="utf-8")
            result = json.loads(stdout)
            assert result["exitcode"] == child.returncode
            results.append(result["exitcode"])
        assert sorted(results) == [0, 5]
        assert not (target / "LookUp.lock").exists()
        cases.append({"name": "concurrent", "expected": [0, 5], "actual": sorted(results)})
    assert sha(BACKUP) == EXPECTED
    evidence = {"cases": cases, "fixture": str(folder), "full": args.full, "strict": args.strict,
                "physical_validation": "ingenieurmodell_nicht_validiert", "original_backup_sha256": EXPECTED}
    (folder / "test_evidence.json").write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    print(json.dumps(evidence, ensure_ascii=False))


if __name__ == "__main__":
    main()
