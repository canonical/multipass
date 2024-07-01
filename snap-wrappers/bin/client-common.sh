#!/bin/sh

export SNAP_REAL_HOME=$( getent passwd $USER | cut -d: -f6 )
