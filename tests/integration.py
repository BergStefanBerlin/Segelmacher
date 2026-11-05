"""Isolated native-CLI contracts. No pytest or external Python packages needed."""
import argparse
import csv
import hashlib
import json
import math
import pathlib
import re
import signal
import shutil
import subprocess
import threading
import time
import uuid


ROOT = pathlib.Path(__file__).resolve().parents[1]
EXPECTED = "e8c47ffca62459c08ae525320794b19884a72544a2857c77606473ccea862eb2"
BACKUP = ROOT / "Backups" / "420_main" / "main_20261105_195154_358.json"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=pathlib.Path, required=True)
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
        shutil.copyfile(ROOT / "documentation" / "aerodynamik_konventionen.json", p / "konventionen.json")
        job = json.loads((ROOT / "examples" / "Auftragsdatei.json").read_text(encoding="utf-8"))
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

    def invoke(p, command="lookup", expected=0, extra=(), cwd=None):
        cmd = [str(exe), command, "--auftrag", str(p / "Auftragsdatei.json"), *extra]
        result = subprocess.run(cmd, cwd=cwd or ROOT, capture_output=True, encoding="utf-8", timeout=1800)
        (p / (command + ".stderr.log")).write_text(result.stderr, encoding="utf-8")
        (p / (command + ".stdout.json")).write_text(result.stdout, encoding="utf-8")
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
        "lookup.json", "summen.csv", "profile.csv", "kraefte.csv", "pruefbericht.json", "auftrag_ausgefuehrt.json"
    }
    meta = json.loads((output / "lookup.json").read_text(encoding="utf-8"))
    assert meta["qualitaet"] == "ingenieurmodell_nicht_validiert"
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
        with (output / "summen.csv").open(encoding="utf-8", newline="") as stream:
            for row in csv.DictReader(stream):
                key = (row["form_id"], float(row["alpha_deg"]), float(row["windneigung_deg"]), float(row["re_ref"]))
                assert key not in sums
                assert previous is None or key > previous
                previous = key
                f = [float(row[k]) for k in ("CFx", "CFy", "CFz")]
                m = [float(row[k]) for k in ("CMx", "CMy", "CMz")]
                assert all(math.isfinite(x) for x in f + m)
                if key[2] == 90:
                    polekey = key[0], key[3]
                    assert polekey not in poles or poles[polekey] == f + m
                    poles[polekey] = f + m
                sums[key] = (f, m)
                if row["angriff_gueltig"] == "1":
                    r = [float(row[k]) / meta["bezuege"]["c_ref_m"] for k in ("angriff_x_m", "angriff_y_m", "angriff_z_m")]
                    rest = [float(row[k]) for k in ("CM_rest_x", "CM_rest_y", "CM_rest_z")]
                    cross = [r[1]*f[2]-r[2]*f[1], r[2]*f[0]-r[0]*f[2], r[0]*f[1]-r[1]*f[0]]
                    assert max(abs(m[i]-cross[i]-rest[i]) for i in range(3)) < 1e-10
                else:
                    assert all(row[k] == "" for k in ("angriff_x_m", "angriff_y_m", "angriff_z_m"))
        expected_rows = len(support_angles) * len(support_elevations) * len(job["betriebspunkte"]["reynoldszahlen_referenz"])
        if args.full:
            assert expected_rows >= 21600
        assert len(support_elevations) >= 10
        assert len(sums) == expected_rows, len(sums)
        integrated = {}
        with (output / "kraefte.csv").open(encoding="utf-8", newline="") as stream:
            for row in csv.DictReader(stream):
                key = row["form_id"], float(row["alpha_deg"]), float(row["windneigung_deg"]), float(row["re_ref"])
                f = [float(row[k]) for k in ("CFx", "CFy", "CFz")]
                r = [float(row[k]) / meta["bezuege"]["c_ref_m"] for k in ("ansatz_x_m", "ansatz_y_m", "ansatz_z_m")]
                m = [float(row[k]) for k in ("CM_eigen_x", "CM_eigen_y", "CM_eigen_z")]
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
        with (output / "profile.csv").open(encoding="utf-8", newline="") as stream:
            for row in csv.DictReader(stream):
                key = row["profil_id"], float(row["alpha_deg"]), float(row["re"])
                assert key[0] in profile_ids
                assert previous is None or key > previous
                previous = key
                assert all(math.isfinite(float(row[k])) for k in ("CL", "CD", "CM"))
        before = {name: sha(output / name) for name in meta["dateien_sha256"]}
        invoke(p, expected=5)
        assert before == {name: sha(output / name) for name in before}
        # A copied result is self-contained.
        assert json.loads((output / "auftrag_ausgefuehrt.json").read_text(encoding="utf-8"))["geometrie"]
        if not args.full:
            for budget in (1, 4):
                limited, _ = fixture("cpu_" + str(budget), budget)
                invoke(limited)
                assert sha(limited / "LookUp" / "summen.csv") == sha(output / "summen.csv")
                report = json.loads((limited / "LookUp" / "pruefbericht.json").read_text(encoding="utf-8"))
                assert report["gpu"]["backend"] == "CPU"
                assert report["cpu_effektiv"] <= budget
                assert 1 <= report["cpu_peak"] <= report["cpu_effektiv"]
            if gpu["backend"] == "OpenCL":
                with (output / "profile.csv").open(encoding="utf-8", newline="") as gpu_stream, \
                     (limited / "LookUp" / "profile.csv").open(encoding="utf-8", newline="") as cpu_stream:
                    gpu_rows, cpu_rows = csv.DictReader(gpu_stream), csv.DictReader(cpu_stream)
                    for gpu_row, cpu_row in zip(gpu_rows, cpu_rows):
                        assert (gpu_row["profil_id"], gpu_row["alpha_deg"], gpu_row["re"]) == \
                               (cpu_row["profil_id"], cpu_row["alpha_deg"], cpu_row["re"])
                        for key in ("CL", "CD", "CM"):
                            g, c = float(gpu_row[key]), float(cpu_row[key])
                            assert abs(g-c) <= 2e-12+2e-12*abs(c)
                    assert next(gpu_rows, None) is None and next(cpu_rows, None) is None
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
        reference_job = json.loads((ROOT / "examples" / "Auftragsdatei.json").read_text(encoding="utf-8"))
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
