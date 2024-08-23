import argparse
import os
from contextlib import asynccontextmanager
from fastapi import FastAPI, Request
from fastapi.responses import Response
import uvicorn
import asyncio
from ctl_plane.terminal.utils import print_info

from figure_9 import run_once as figure_9_run_once
from figure_10 import run_once as figure_10_run_once
from figure_11 import run_once as figure_11_run_once
from figure_12 import run_once as figure_12_run_once
from table_1 import run_once as table_1_run_once


@asynccontextmanager
async def lifespan(app: FastAPI):
    global GSTATE
    GSTATE = "IDLE"
    yield


app = FastAPI(lifespan=lifespan)
global GSTATE
global RESULTS

RESULTS = {}


@app.post("/figure/{id}")
async def post_figure(id: int, req: Request) -> Response:
    """Gen new figure."""
    assert id in [9, 10, 11, 12]
    name = f"figure_{id}"
    global GSTATE, RESULTS
    if GSTATE == "IDLE":
        GSTATE = "BUSY"
        req_json = await req.json()

        async def run_figure():
            global RESULTS, GSTATE
            try:
                if id == 9:
                    print("Running figure 9")
                    print(f"{name}")
                    RESULTS[name] = await figure_9_run_once(req_json)
                elif id == 10:
                    print("Running figure 10")
                    print(f"{name}")
                    RESULTS[name] = await figure_10_run_once(req_json)
                elif id == 11:
                    print("Running figure 11")
                    print(f"{name}")
                    RESULTS[name] = await figure_11_run_once(req_json)
                elif id == 12:
                    print("Running figure 12")
                    print(f"{name}")
                    RESULTS[name] = await figure_12_run_once(req_json)
            except Exception as e:
                print(f"error: {e}")
            finally:
                GSTATE = "IDLE"

        asyncio.get_event_loop().create_task(run_figure())
        return Response(status_code=200)
    else:
        return Response(status_code=503)


@app.get("/figure/{id}")
async def get_figure(id: int, req: Request) -> Response:
    """Gen new figure."""
    assert id in [9, 10, 11, 12]
    name = f"figure_{id}"
    global GSTATE, RESULTS
    if GSTATE == "BUSY":
        return Response(status_code=503)
    elif GSTATE == "IDLE":
        if name not in RESULTS:
            return Response(status_code=404)
        return RESULTS[name]
    else:
        return Response(status_code=501)


@app.delete("/figure/{id}")
async def delete_figure(id: int, req: Request) -> Response:
    """Del a figure."""
    assert id in [9, 10, 11, 12]
    name = f"figure_{id}"
    if name in RESULTS:
        del RESULTS[name]
        return Response(status_code=200)
    else:
        return Response(status_code=404)


@app.post("/table/{id}")
async def post_table(id: int, req: Request) -> Response:
    """Gen new table."""
    assert id in [1]
    name = f"table_{id}"
    global GSTATE, RESULTS
    if GSTATE == "IDLE":
        GSTATE = "BUSY"
        req_json = await req.json()

        async def run_figure():
            global RESULTS, GSTATE
            try:
                if id == 1:
                    print("Running table 1")
                    print(f"{name}")
                    RESULTS[name] = await table_1_run_once(req_json)
            except Exception as e:
                print(f"error: {e}")
            finally:
                GSTATE = "IDLE"

        asyncio.get_event_loop().create_task(run_figure())
        return Response(status_code=200)
    else:
        return Response(status_code=503)


@app.get("/table/{id}")
async def get_table(id: int, req: Request) -> Response:
    """Gen new table."""
    assert id in [1]
    name = f"table_{id}"
    global GSTATE, RESULTS
    if GSTATE == "BUSY":
        return Response(status_code=503)
    elif GSTATE == "IDLE":
        if name not in RESULTS:
            return Response(status_code=404)
        return RESULTS[name]
    else:
        return Response(status_code=501)


@app.delete("/table/{id}")
async def delete_figure(id: int, req: Request) -> Response:
    """Del a table."""
    assert id in [9, 10, 11]
    name = f"table_{id}"
    if name in RESULTS:
        del RESULTS[name]
        return Response(status_code=200)
    else:
        return Response(status_code=404)


def parse_args():
    parser = argparse.ArgumentParser(
        prog=os.path.basename(__file__), description=f"Run program in {__file__}"
    )
    parser.add_argument("-H", "--host", type=str, default="0.0.0.0")
    parser.add_argument("-p", "--port", type=int, default=8080)
    args = parser.parse_args()
    return args


if __name__ == "__main__":
    args = parse_args()
    print_info(args)
    uvicorn.run(
        app,
        host=args.host,
        port=args.port,
        log_level="info",
        timeout_keep_alive=60,
        loop="asyncio",
    )
