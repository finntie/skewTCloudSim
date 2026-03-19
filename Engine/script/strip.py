import vcproj.solution
import vcproj.project
import os

def strip_from_solution(solution_filename, files_to_remove):
    solution_file = solution_filename
    solution = vcproj.solution.parse(solution_file)
    for project_file in solution.project_files():
        project = vcproj.project.parse(os.path.join(os.path.dirname(solution_file), project_file))

        for source_file in files_to_remove:
            if source_file in project.source_files():
                project.remove_source_file(source_file)
                if source_file in project.source_files():
                    print("removing file: " + source_file + " from: " + project_file + "... failed..!")
                else:
                    print("removing file: " + source_file + " from: " + project_file + "... success..!")
        
        for include_file in files_to_remove:
            if include_file in project.include_files():
                project.remove_include_file(include_file)
                if include_file in project.include_files():
                    print("removing file: " + include_file + " from: " + project_file + "... failed..!")
                else:
                    print("removing file: " + include_file + " from: " + project_file + "... success..!")
        
        project.write(project.filename)
    
    return

def strip_from_disc(files_to_remove):
    for file in files_to_remove:
        fullpath = ("..\\" + file).replace("\\", "/")
        if os.path.exists(fullpath.strip()):
            os.remove(fullpath)

    return