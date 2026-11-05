import os
import subprocess

import pytest

from harness import CHANNEL, OFF, VCU_BIN, Harness, channel_is_quiet


@pytest.fixture
def bench():
    if not os.path.exists(f"/sys/class/net/{CHANNEL}"):
        pytest.fail(f"{CHANNEL} absent : lancer bash scripts/setup_vcan.sh")
    if not VCU_BIN.exists():
        pytest.fail(f"{VCU_BIN} absent : cmake -S . -B build && cmake --build build")
    if not channel_is_quiet():
        pytest.fail("Du trafic est déjà présent sur vcan0 : arrête ./build/vcu et les nœuds Python")

    harness = Harness()
    vcu = subprocess.Popen(
        [str(VCU_BIN), CHANNEL], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    try:
        harness.wait_state(OFF, 3.0)
        yield harness
    finally:
        vcu.terminate()
        try:
            vcu.wait(timeout=3)
        except subprocess.TimeoutExpired:
            vcu.kill()
        harness.close()