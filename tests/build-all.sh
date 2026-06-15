#echo "++ x86-32 ++"
#clang -o hello32.bin -rdynamic -m32 ./hello_world.c
echo "++ x86-64 ++"
clang -pie -o hello64.bin -rdynamic ./hello_world.c
echo "++ x86-64 nopie ++"
clang -no-pie -o hello64_nopie.bin -rdynamic ./hello_world.c
echo "++ x86-64 static ++"
clang -static -o hello64_static.bin ./hello_world.c #-Wl,-static #-Wl,--no-dynamic-linker
echo "++ x86-64 static nostd ++"
clang -static -nostdlib -o hello64_static_nostart.bin ./hello_world_nostd.c #-Wl,-static #-Wl,--no-dynamic-linker
echo "++ x86-64 static pie ++"
clang -static-pie -o hello64_static_pie.bin ./hello_world.c #-Wl,-static #-Wl,--no-dynamic-linker
echo "++ x86-64 shared ++"
clang -shared -o hello64_shared.bin ./hello_world.c
