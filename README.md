# gem5-sim

## Build & Install
* Clone the repository including submodules:
    ```bash
    git clone --recursive git@gitlab.ice.rwth-aachen.de:pelke/gem5-sim.git
    ```

* Build gem5:
    ```bash
    python3 -m venv .venv
    source .venv/bin/activate
    pip install -r requirements.txt
    pip install -r gem5/requirements.txt
    ./gem5/util/pre-commit-install.sh
    ```
