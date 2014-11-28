#!/bin/sh


NAME=hello-world

avr-objdump -h -S ${NAME}.elf > ${NAME}.lst
vim ${NAME}.lst
