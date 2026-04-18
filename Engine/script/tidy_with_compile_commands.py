import json
import os
import subprocess
import shutil

from create_compile_commands import (
    parse_arguments,
    create_compile_commands_with_msbuild,
    combine_and_format_json_fragments,
)


def run_clang_tidy(compile_commands, platform: str):
    output_path = os.path.join("tidy", platform)

    # Ensure the 'tidy/{platform}' directory exists
    if os.path.exists(output_path):
        # Remove all contents of the directory
        for item in os.listdir(output_path):
            item_path = os.path.join(output_path, item)
            if os.path.isfile(item_path):
                os.unlink(item_path)
            elif os.path.isdir(item_path):
                shutil.rmtree(item_path)
    else:
        # Create the directory if it doesn't exist
        os.makedirs(output_path, exist_ok=True)

    for item in compile_commands:
        file_path = item["file"]

        # Check if the file is a .cpp file and not in the 'external' directory
        if file_path.endswith((".cpp", ".cc")) and not file_path.find("external") != -1:
            file_name = os.path.basename(file_path)
            file_name, _ = os.path.splitext(file_name)
            output_file = os.path.join(output_path, f"{file_name}.txt")

            # Construct the clang-tidy command
            command = ["clang-tidy", file_path]

            output = ""
            print(f"Running clang-tidy on {file_path}")
            try:
                # Run clang-tidy
                result = subprocess.run(
                    command, check=True, capture_output=True, text=True
                )
                print(f"Clang-tidy completed for {file_path}.")
                output = result.stdout

            except subprocess.CalledProcessError as e:
                print(f"Error running clang-tidy on {file_path}:")
                print(e.output)
                output = e.output

            # If there's any output from clang-tidy, print it and save it to file
            if output:
                print("Output:")
                print(output)
                with open(output_file, "w") as file:
                    file.write(output)
                print(f"Results exported to {output_file}")


if __name__ == "__main__":
    args = parse_arguments()

    create_compile_commands_with_msbuild(args.target, args.platform)
    combine_and_format_json_fragments()

    # Load the compile commands
    with open("compile_commands.json", "r") as f:
        compile_commands = json.load(f)

    # Run clang-tidy on each file
    run_clang_tidy(compile_commands, args.platform)

    print("\nClang-tidy analysis completed for all files.")
