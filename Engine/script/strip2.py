import os
import shutil
import zipfile
import fnmatch

def is_ignored_file(file, ignored_files):
    # replace backslashes with forward slashes
    file = file.replace("\\", "/")        
    for ignored_file in ignored_files:
        # remove the trailing slash
        if ignored_file.endswith("/"):
            ignored_file = ignored_file[:-1]
        # add dot slash to the ignored file
        if not ignored_file.startswith("./"):
            ignored_file = "./" + ignored_file
        # check if the file matches the ignored file
        if fnmatch.fnmatch(file, ignored_file):
            return True        
    return False

def copy_with_filter(src_file, dst_file, filter):
    # Create the destination directory if it does not exist
    if not os.path.exists(os.path.dirname(dst_file)):
        os.makedirs(os.path.dirname(dst_file))

    # check if it's a c++ source file
    if not src_file.endswith(".cpp") and not src_file.endswith(".h") and not src_file.endswith(".hpp"):
        # copy the file
        shutil.copy(src_file, dst_file)        
        return
    
    # Open the source file
    with open(src_file, "r", errors="ignore") as f:
        # Open the destination file
        with open(dst_file, "w") as g:
            skip = False
            # Loop through all lines in the source file
            for line in f:
                # Check if the line contains the filter
                if ("#pragma region " + filter) in line:
                    skip = True                                    
                # Write the line to the destination file if it is not skipped 
                if not skip: 
                    g.write(line)    
                if "#pragma endregion" in line:
                    skip = False                
            # close the destination file
            g.close()
        # close the source file
        f.close()
    return
    
####################################################################################################
# Main
####################################################################################################

# Define the root directory and the ignore file
root_dir = "."
ignore_file = root_dir + "/.gitignore"

# Define the destination directory
dst_dir = root_dir + "/_stripped"
filter = "OPTIMIZATION"

# Create a set of ignored files
with open(ignore_file, "r") as f:
    ignored_files = set(f.read().splitlines())

# add the ignore file to the ignored files set
ignored_files.add(ignore_file)

# add more files to the ignored files set, like the script itself, the destination directory and git files
ignored_files.add(dst_dir)
ignored_files.add(root_dir +  "/script/strip2.py")
ignored_files.add(root_dir +  "/.git")
ignored_files.add(root_dir +  "/.github")
ignored_files.add(root_dir +  "/.gitignore")
ignored_files.add(root_dir +  "/.gitmodules")
ignored_files.add(root_dir +  "/.gitattributes")
ignored_files.add(root_dir +  "/.vs")
ignored_files.add(root_dir +  "/.vscode")
ignored_files.add(root_dir +  "/script")
ignored_files.add(root_dir +  "/build")

# create the destination directory, or clear it if it already exists
if os.path.exists(dst_dir):
    shutil.rmtree(dst_dir)
os.makedirs(dst_dir)

# Loop through all files in the root directory recursively
for root, dirs, files in os.walk(root_dir, topdown=True):
    # Remove the ignored directories from the list of directories to walk
    dirs[:] = [d for d in dirs if not is_ignored_file(root + "/" + d, ignored_files)]    
    # Loop through all files in the current directory
    #if is_ignored_file(root, ignored_files):
    #    continue
    for file in files:
        # Check if the file is not in the ignored files set
        if not is_ignored_file(file, ignored_files):
            # Construct the source and destination paths
            src_file = os.path.join(root, file)
            dst_file = os.path.join(dst_dir + root, file)
            # Create the destination directory if it does not exist
            if not os.path.exists(os.path.dirname(dst_file)):
                os.makedirs(os.path.dirname(dst_file))
            # Copy the file to the destination directory
            copy_with_filter(src_file, dst_file, filter)
                

# Create a zip file of the destination directory
zip_file = zipfile.ZipFile(root_dir + "/bee.zip", "w", zipfile.ZIP_DEFLATED)
for root, dirs, files in os.walk(dst_dir):
    for file in files:
        zip_file.write(os.path.join(root, file))
zip_file.close()
