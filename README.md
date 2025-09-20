# CS5390-AdvComp

Inside the BrilClassWork folder is a mycfg.py file that contains the work for the following:
- Basic Blocks and MyCFG program
- Path Length Function
- Reverse Postorder Function
- Back Edge Function
- Is Reducible Function
- Some Test bril files

With that, it will take any bril program and run through the process of extracting the instructions from the program and running it through the functions. Some example bril programs have been provided inside the "./BrilClassWork/tests" folder.

TO RUN:
1. You may copy the ClassWork folder somewhere else, but should work inside the folder copied from the bril repository
2. Run with: bril2json < ./path/to/tests/example.bril | python3 ./path/to/mycfg.py
3. Example (if inside bril repo): bril2json < ../tests/branch_jmp.bril | python3 ../mycfg.py

Refer to the bril repo for troubleshooting with bril itself: https://github.com/sampsyo/bril.git
