#!/bin/sh

avr-objdump -h -S $@.elf > $@.lst
