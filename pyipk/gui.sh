#!/bin/bash
env PYTHONPATH="${PYTHONPATH}:." python ./bin/ipkqt --load ./plugins/ $@
