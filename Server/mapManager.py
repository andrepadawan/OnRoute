import uuid
from typing import List
from pathlib import Path
from pydantic import BaseModel, Field, TypeAdapter

POI_FILE = Path(__file__).parent / "pointsOfInterest.json"

class PointOfInterest(BaseModel):
    id: str = Field(default_factory = lambda : str(uuid.uuid4()))
    name: str = Field(default_factory = lambda : '')
    lat: float
    lon: float

_adapter = TypeAdapter(List[PointOfInterest])

class MapManager():
    def __init__(self):
        pass

    def addPointsOfInterest(self, POI: PointOfInterest):
        data = self.readPointsOfInterest()
        data.append(POI)
        self.writePointsOfInterest(data)

    def deletePointsOfInterest(self, id: str):
        data = self.readPointsOfInterest()
        for t in data:
            if t.id == id:
                data.remove(t)
        self.writePointsOfInterest(data)

    def readPointsOfInterest(self) -> List[PointOfInterest]:
        try:
            with open(POI_FILE, "r") as f:
                data = f.read()
                if not data.strip() or data == "null":
                    return []
                return _adapter.validate_json(data)
        except FileNotFoundError:
            return []

    def returnPrimitiveTypeList(self) -> List[dict]:
        data = self.readPointsOfInterest()
        return _adapter.dump_python(data)


    def writePointsOfInterest(self, data: List[PointOfInterest]):
        with open(POI_FILE, "wb") as f:
            f.write(_adapter.dump_json(data))