"""Re-express a rig file in a world frame turned 90 degrees about Y: (x, y, z) -> (z, y, -x)."""
import json, sys
import numpy as np, cv2
RW = np.array([[0.0, 0, 1], [0, 1, 0], [-1, 0, 0]])
if __name__ == "__main__":
    path = sys.argv[1]
    rec = json.load(open(path))
    for shot in rec[0]["shots"].values():
        R = cv2.Rodrigues(np.array(shot["rotation"], dtype=float))[0] @ RW.T
        shot["rotation"] = [float(v) for v in cv2.Rodrigues(R)[0].ravel()]
    for point in rec[0]["points"].values():
        point["coordinates"] = [float(v) for v in RW @ np.array(point["coordinates"])]
    json.dump(rec, open(path, "w"), indent=2)
    print("rig rotated 90 degrees about Y")
