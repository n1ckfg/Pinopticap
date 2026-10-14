"""rig_edit.py RIG.json bump HOST DEGREES | drop HOST -- make the calibration differ from the real rig:
turn one camera about the world Y axis through its own centre, or remove it."""
import json, math, os, sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "camera_sim"))
import camera_sim as cs
path, what, host = sys.argv[1:4]
rig = json.load(open(path))
key = next(k for k in rig[0]["shots"] if k.split("_")[0] == host)
if what == "drop":
    del rig[0]["shots"][key]
else:
    mm = lambda a, b: tuple(tuple(sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)) for i in range(3))
    shot = rig[0]["shots"][key]
    D = cs.rvec_to_rotation(cs.scale((0.0, 1.0, 0.0), math.radians(float(sys.argv[4]))))
    shot["rotation"] = list(cs.rotation_to_rvec(mm(D, cs.rvec_to_rotation(tuple(shot["rotation"])))))
    shot["translation"] = list(cs.matvec(D, tuple(shot["translation"])))
json.dump(rig, open(path, "w"))
print("rig edit:", what, host, *sys.argv[4:])
