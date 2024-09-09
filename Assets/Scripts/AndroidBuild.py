import os
import subprocess
import shutil
import argparse

def get_file_extension(file_path):
    _, extension = os.path.splitext(file_path)
    return extension # returns .text


def run_command_in_directory(command, directory):
    try:
        # Open subprocess with the specified command and directory
        process = subprocess.Popen(command, shell=True, cwd=directory, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        # Wait for the process to finish and capture output
        stdout, stderr = process.communicate()
        
        # Decode the output
        stdout_str = stdout.decode()
        stderr_str = stderr.decode()
        
        # Print the output
        print("stdout:", stdout_str)
        print("stderr:", stderr_str)
    except Exception as e:
        print("Error:", e)
        print("dir:", directory)


def ensure_directory_exists(directory_path):
    if not os.path.exists(directory_path):
        os.makedirs(directory_path)
        print(f"Created directory: {directory_path}")


def copy_files(source_folder, destination_folder, extension=None):
    # Check if source and destination folders exist
    if not os.path.exists(source_folder):
        print(f"Source folder '{source_folder}' does not exist.")
        return
    
    # Create destination folder if it doesn't exist
    ensure_directory_exists(destination_folder)
    
    # Traverse the directory tree using os.walk() to recursively copy files
    for root, dirs, files in os.walk(source_folder):
        for file in files:
            source_file = os.path.join(root, file)
            
            # Check file extension if extension filter is specified
            if extension is not None and get_file_extension(source_file) != extension:
                continue  # Skip this file if it doesn't match the extension filter
            
            relative_path = os.path.relpath(source_file, source_folder)
            destination_file = os.path.join(destination_folder, relative_path)
            
            # Create intermediate directories if necessary
            ensure_directory_exists(os.path.dirname(destination_file))
            
            if os.path.exists(destination_file):
                print(f"Replacing '{relative_path}' in destination folder.")
            else:
                print(f"Copying '{relative_path}' to destination folder..")
            
            shutil.copy(source_file, destination_file)
    
    print("Files copied successfully.")

#------------------------------------------------------------------------
def AndroidMoveShaders():
    print("Moving shader files to Android path..")
    workspacePath = os.getcwd()  # Assuming this retrieves the workspace path
    
    # Define source and destination paths
    sourcePath = os.path.join(workspacePath, "Assets/Shaders")
    destPath   = os.path.join(workspacePath, "Android/app/src/main/assets/Shaders")
    assetsPath = os.path.join(workspacePath, "Android/app/src/main/assets/Assets")
    ensure_directory_exists(assetsPath)
    ensure_directory_exists(destPath)
    copy_files(sourcePath, destPath, ".glsl")


def AndroidMoveFontFiles():
    print("Moving Font files to Android path..")
    workspacePath = os.getcwd()  # Assuming this retrieves the workspace path
    sourcePath = os.path.join(workspacePath, "Assets/Fonts")
    destPath   = os.path.join(workspacePath, "Android/app/src/main/assets/Assets/Fonts")
    ensure_directory_exists(destPath)
    copy_files(sourcePath, destPath, ".bft")


def AndroidMoveAudioFiles():
    print("Moving Font files to Android path..")
    workspacePath = os.getcwd()  # Assuming this retrieves the workspace path
    sourcePath = os.path.join(workspacePath, "Assets/Audio")
    destPath   = os.path.join(workspacePath, "Android/app/src/main/assets/Assets/Audio")
    ensure_directory_exists(destPath)
    copy_files(sourcePath, destPath, ".mp3")
    copy_files(sourcePath, destPath, ".wav")
    copy_files(sourcePath, destPath, ".ogg")


def AndroidMoveMeshes():
    print("Moving mesh and astc files to Android path..")
    workspacePath = os.getcwd()  # Assuming this retrieves the workspace path
    # Define source and destination paths
    sourcePath = os.path.join(workspacePath, "Assets/Meshes")
    destPath   = os.path.join(workspacePath, "Android/app/src/main/assets/Assets/Meshes")
    assetsPath = os.path.join(workspacePath, "Android/app/src/main/assets/Assets")
    ensure_directory_exists(assetsPath)
    ensure_directory_exists(destPath)
    copy_files(sourcePath, destPath, ".abm")
    copy_files(sourcePath, destPath, ".astc")


def AndroidMoveTextures():
    print("Moving texture files to Android path..")
    workspacePath = os.getcwd()  # Assuming this retrieves the workspace path
    # Define source and destination paths
    sourcePath = os.path.join(workspacePath, "Assets/Textures")
    destPath   = os.path.join(workspacePath, "Android/app/src/main/assets/Assets/Textures")
    assetsPath = os.path.join(workspacePath, "Android/app/src/main/assets/Assets")
    ensure_directory_exists(assetsPath)
    ensure_directory_exists(destPath)
    copy_files(sourcePath, destPath, ".png")
    copy_files(sourcePath, destPath, ".jpg")

def AndroidMoveAllAssets():
    AndroidMoveShaders()
    AndroidMoveFontFiles()
    AndroidMoveAudioFiles()
    AndroidMoveMeshes()
    AndroidMoveTextures()

#------------------------------------------------------------------------
# you have to install android studio to run this
def BuildAndroid():
    print("Android build running please wait..")
    workspacePath = os.getcwd()
    run_command_in_directory("gradlew assembleDebug", os.path.join(workspacePath, "Android"))


def RunAndroid():
    workspacePath = os.getcwd()
    path = os.path.join(workspacePath, "Android/app/release/")
    apk_path = os.path.join(workspacePath, "Android/app/build/outputs/apk/debug/app-debug.apk")
    run_command_in_directory("adb install " + apk_path, workspacePath)
    run_command_in_directory("adb shell am start -n com.anilgames.game/.MainActivity", path) 

def main():
    # Define command-line arguments using argparse
    parser = argparse.ArgumentParser(description="Script for performing various actions.")
    parser.add_argument('action', choices=['build', 'run', 'move_assets', 'move_shaders'], help="Action to perform (build, run, move_assets, move_shaders)")
    args = parser.parse_args()

    # Perform action based on the provided command-line arguments
    if args.action == 'build':
        BuildAndroid()
    elif args.action == 'run':
        RunAndroid()
    elif args.action == 'move_shaders':
        AndroidMoveShaders()
    elif args.action == 'move_assets':
        AndroidMoveAllAssets()

if __name__ == '__main__':
    main()