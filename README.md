# File Redirector
Plugin that reads all files under folder name `RedirectorData` and when game attempts to load file called for example `Data\SomeFile.bin` and there is already file `RedirectorData\SomeFile.bin` it will simply redirect loading the file from disk rather than `.big` file.

## Installation
> [!IMPORTANT]  
> Please read README.md on organization profile for getting required game executable for compatibility.

1. Download required files here: [FileRedirector.zip](https://github.com/SDmodding/FileRedirector/releases/latest/download/FileRedirector.zip)
2. Extract all files in game folder.
3. Now create folder called `RedirectorData`.
4. Inside this folder you put any files you wanna redirect:
    - You will need to create folder to match path for the file you're redirecting:
        - `Data\UI\Icons_Weapon_GROCERYBAG.perm.bin` -> `RedirectorData\UI\Icons_Weapon_GROCERYBAG.perm.bin`
    - All files that have extension `.perm.bin` requires `.temp.bin` to be created also even if it's empty!
5. If your file isn't redirecting correctly after matching same folder names and file names then open issue at this repository and explain the issue you're having.