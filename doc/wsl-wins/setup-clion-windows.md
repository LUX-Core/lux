# Getting started with WSL

Let’s start with the simple instruction on how to set up the WSL environment in CLion:

Note: Your Windows file system is located at /mnt/c in the Bash shell environment.

1.Download and install WSL distribution (for instance, Ubuntu) from Microsoft Store
    
    https://www.microsoft.com/en-us/p/ubuntu/9nblggh4msv6
    
2.Run Ubuntu. Note: upon the first lunch of Ubuntu the system may prompt you to enable the Windows optional feature. In this case, you need to pen Windows Power Shell as Administrator and run the following: 

    Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Windows-Subsystem-Linux
    
Restart your computer.

3.Create a new user and specify your user name and password. 

4.Set up WSL Ubuntu environment (or you can use depends/install-dependencies.sh)

    sudo apt-get install cmake gcc clang gdb build-essential
    
Configure and run open ssh-server. 

    wget https://raw.githubusercontent.com/JetBrains/clion-wsl/master/ubuntu_setup_env.sh && bash ubuntu_setup_env.sh
    
5.Now you can run CLion and create a toolchain for WSL:     

Open clion -> file -> settings -> toolchains -> create WSL toolchains -> configure retmote credentials:

    Host: localhost
    Port:2222
    Username: 
    Password:
-------------------------------------------
## Enable use wsl bash as terminal in Clion

### Wins 10

Copy 

    c:\Windows\System32\bash.exe
    
Paste

    c:\bash.exe
    
Open clion -> file -> settings -> tools -> Terminal -> Application settings -> Shell path: c:\bash.exe

### Wins 8.1 ( path including double quotes)

Open clion -> file -> settings -> tools -> Terminal -> Application settings -> Shell path: "C:\Program Files\Git\bin\sh.exe" -login -i

### Another way around is to call ubuntu executable directly

Open clion -> file -> settings -> tools -> Terminal -> Application settings -> Shell path: C:\Users\<my_user>\AppData\Local\Microsoft\WindowsApps\ubuntu.exe

## Current issues and limitations

Due to the IntelliJ platform issue, there are problems with using WSL file-system which is case-sensitive and Windows file system which is not. As a workaround, you can do the following:

    Go to Help | Edit Custom properties... menu option and specify
    idea.case.sensitive.fs=true
    Invalidate caches and restart the IDE using File | Invalidate Caches and Restart menu option. 
    
ENJOY !!!
