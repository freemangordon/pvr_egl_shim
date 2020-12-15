#!/bin/sh

mkfifo fifo0 fifo1
./glamor_srv > fifo0 < fifo1 &
./glamor_cli < fifo0 > fifo1
