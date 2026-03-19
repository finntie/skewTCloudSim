# Copy dependencies of non-open-source libraries to the external directory
import os
from distutils.dir_util import copy_tree

# 1 - Copy Superluminal dependencies
def copy_superluminal():
    superluminal_dir_include = os.path.join("C:\Program Files\Superluminal\Performance\API\include\Superluminal")
    superluminal_dir_lib = os.path.join("C:\Program Files\Superluminal\Performance\API\lib\\x64")
    superluminal_dir_dll = os.path.join("C:\Program Files\Superluminal\Performance\API\dll\\x64")
    superluminal_dir_to = os.path.join("external/Superluminal/")
    os.makedirs(superluminal_dir_to, exist_ok=True)

    copy_tree(superluminal_dir_include, superluminal_dir_to)
    copy_tree(superluminal_dir_lib, superluminal_dir_to)
    copy_tree(superluminal_dir_dll, superluminal_dir_to)


# 2 - Copy FMod dependencies
def copy_fmod():
    # TODO: Implement this function for all platforms (Windows and PS5 at current)
    print("FMod dependencies TODO!")
    
# Create UI to ask for user input
def main():
    print("Copy dependencies of non-open-source libraries to the external directory")
    print("1. Superluminal")
    print("2. FMod")
    print("3. Exit")
    choice = int(input("Enter your choice: "))
    if choice == 3:
        exit()
    elif choice == 1:
        copy_superluminal()
    elif choice == 2:
        copy_fmod()

main()
