from fastapi import FastAPI

app = FastAPI()

#the endpoint to post coordinates to main site
@app.get("/coordinates")
async def get_coordinates():
    #posts coordinates
    pass

@app.get("/admin")
async def admin_page():
    #displays admin page
    pass

@app.post("/update-coordinates")
async def update_coordinates():
    #receives from Raspberry
    pass


