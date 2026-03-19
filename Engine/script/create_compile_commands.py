import os
import glob
import json
import subprocess
import argparse

flags_to_remove = ["-xc++", "-ffp-model=precise", "-O0", "-fno-fast-math"]


def combine_and_format_json_fragments(
    compile_commands_path="compile_commands", output_file="compile_commands.json"
):
    # Get list of JSON files in the directory
    json_files = glob.glob(os.path.join(compile_commands_path, "*.json"))

    combined_data = []

    for file_path in json_files:
        with open(file_path, "r") as infile:
            content = infile.read().strip()

            # Remove the trailing comma if it exists
            if content.endswith(","):
                content = content[:-1]

            try:
                data = json.loads(content)

                if "arguments" in data:
                    # Remove the specified flags from the arguments list
                    data["arguments"] = [
                        arg
                        for arg in data["arguments"]
                        if arg not in flags_to_remove
                        and not arg.startswith("-clang:-MJc")
                    ]

                combined_data.append(data)
            except json.JSONDecodeError as e:
                print(f"Error decoding JSON in {file_path}: {e}")
                print("Skipping this file.")

        # Delete the file after processing
        os.remove(file_path)

    # Write the combined and formatted data to the output file
    with open(output_file, "w") as outfile:
        json.dump(combined_data, outfile, indent=2)

    os.rmdir(compile_commands_path)

    print(f"Compilation database created: {output_file}")


def create_compile_commands_with_msbuild(
    solution: str, platform: str, compile_commands_path: str = "compile_commands"
):
    if not os.path.exists(compile_commands_path):
        os.mkdir(compile_commands_path)

    command = f'msbuild "{solution}" -noLogo -t:ClCompile /p:Configuration="Debug" /p:Platform="{platform}" -p:BeeLint=true'
    subprocess.run(command, check=True, capture_output=True, text=True)


def parse_arguments():
    parser = argparse.ArgumentParser(description="Run MSBuild with specified platform.")
    parser.add_argument(
        "--platform",
        default="x64",
        choices=["x64", "Prospero"],
        help="Specify the platform (x64 or Prospero)",
    )
    parser.add_argument(
        "--target",
        default="bee.sln",
        help="sln/vcxproj file that should be used for the build.",
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_arguments()

    create_compile_commands_with_msbuild(args.target, args.platform)
    combine_and_format_json_fragments()
