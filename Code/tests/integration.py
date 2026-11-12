"""Isolated native-CLI and Parquet contracts using PyArrow."""
import argparse
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
import threading
import time
import uuid
import pyarrow as pa
import pyarrow.parquet as pq


ROOT = pathlib.Path(__file__).resolve().parents[2]
EXPECTED = "e8c47ffca62459c08ae525320794b19884a72544a2857c77606473ccea862eb2"
BACKUP = ROOT / "Code" / "tests" / "daten" / "main_20261105_195154_358.json"
PARQUET_FILES = ("summen.parquet", "kraefte.parquet", "profile.parquet")


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
    parser.add_argument("--parquet-reader", type=pathlib.Path, required=True)
    parser.add_argument("--model-schemas", type=pathlib.Path, required=True)
    parser.add_argument("--full", action="store_true")
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()
    exe = args.exe.resolve()
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
        cmd = [str(exe), command, "--auftrag", str(p / "Auftragsdatei.json"), *extra]
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

    p, job = fixture("420 räumlich")
    invoke(p, "pruefen", cwd=folder)
    invoke(p, "pruefen", cwd=p)
    _, log = invoke(p)
    output = p / "LookUp"
    assert set(f.name for f in output.iterdir()) == {
        "lookup.json", *PARQUET_FILES, "pruefbericht.json", "auftrag_ausgefuehrt.json"
    }
    meta = json.loads((output / "lookup.json").read_text(encoding="utf-8"))
    assert meta["formatversion"] == "2"
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
        assert set(range(0, 91, 10)).issubset(set(support["basis_windneigung_deg"]))
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
        if key[2] == 90:
            polekey = key[0], key[3]
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
        (model / ".modells_validation.lock").mkdir()
        failed, _ = invoke(aero, extra=("--fingerprint", "update"), env=env, expected=5)
        assert "CORE-FPR-0002" in failed["meldung"]
        assert validation_file.read_bytes() == validation_bytes
        failed_after_write, _ = invoke(
            aero, extra=("--output-policy", "overwrite", "--fingerprint", "update"),
            env=env, expected=5)
        assert failed_after_write["output_action"] == "written"
        assert pathlib.Path(failed_after_write["ergebnisordner"]) == aero / "LookUp"
        assert validation_file.read_bytes() == validation_bytes
        (model / ".modells_validation.lock").rmdir()
        written, _ = invoke(aero, extra=("--output-policy", "overwrite", "--fingerprint", "update"), env=env)
        assert written["output_action"] == "overwritten"
        stable, _ = invoke(aero, extra=("--fingerprint", "update"), env=env)
        assert stable["fingerprint"]["updated"] is False
        assert stable["fingerprint"]["hash"] == written["fingerprint"]["hash"]
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
