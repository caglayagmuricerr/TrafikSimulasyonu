#!/bin/bash

script="scratch/example"

nDevices=100     # Change from 50 to 200
MobileNodeProbability=1
MinSpeed=5       # Change from 5 to 20
MaxSpeed=5       # Change from 5 to 20

folder="scratch/example/Test_${MaxSpeed}_${nDevices}/run"

echo -n "Running example: "

for r in `seq 1 30`; do
  echo -n " $r"
  mkdir -p ${folder}${r}
  
  ./ns3 run "${script} --RngRun=${r} --nDevices=${nDevices} --MobileNodeProbability=${MobileNodeProbability} --MinSpeed=${MinSpeed} --MaxSpeed=${MaxSpeed} --OutputFolder=${folder}${r}" > "${folder}${r}/log.txt" 2>&1
done

echo " END"
