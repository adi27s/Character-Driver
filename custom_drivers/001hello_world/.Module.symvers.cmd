cmd_/home/aditya/workspace/ldd/custom_drivers/001hello_world/Module.symvers := sed 's/ko$$/o/' /home/aditya/workspace/ldd/custom_drivers/001hello_world/modules.order | scripts/mod/modpost -m    -o /home/aditya/workspace/ldd/custom_drivers/001hello_world/Module.symvers -e -i Module.symvers   -T -