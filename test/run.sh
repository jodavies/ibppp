#!/bin/bash

# "doublebox" are tables which come from the example reductions with FIRE
../bin/ibppp --fire-table tables/doublebox.tables.gz --form-fill output/fill-doublebox.h.gz --cpus 1 --vars d,s,t --f-lhs db --f-rhs midb
