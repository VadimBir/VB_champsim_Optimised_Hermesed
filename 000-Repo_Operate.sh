#!/usr/bin/env bash
set -e

# sudo du -h --max-depth=5 / \
#   --exclude=/dev \
#   --exclude=/home/cc/Champsim_V4/traces \
#   --exclude=/home/cc/.cache \
#   --exclude=/home/cc/.vscode-server \
#   --exclude=/var/log/journal \
#   --exclude=/opt/llvm-21.1.0 \
#   2>/dev/null | sort -h

# ============================================================================
# DATA PROCESS - NO FILES, USE /proc TO READ ENVIRONMENT
# ============================================================================

# used to store the login email and repo URL in process, and run indefinitely till pursued or rebooted
PROCESS_NAME="champsim_data_keeper" 

start_data_process() {
    local repo_url="$1"
    local repo="$2"
    local repo_folder="$3"
    local ssh_key="$4"
    local git_user="$5"
    local git_email="$6"
    
    (
        export REPO_URL="$repo_url"
        export REPO="$repo"
        export REPO_FOLDER="$repo_folder"
        export SSH_KEY_PATH="$ssh_key"
        export GIT_USERNAME="$git_user"
        export GIT_EMAIL="$git_email"
        
        exec -a "$PROCESS_NAME" sleep infinity
    ) &
    
    echo "[+] Data process started (PID: $!)"
}

get_data_pid() {
    ps aux | grep "$PROCESS_NAME" | grep -v grep | awk '{print $2}' | head -n1
}

check_data_process() {
    local pid=$(get_data_pid)
    [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null
}

kill_data_process() {
    local pid=$(get_data_pid)
    if [[ -n "$pid" ]]; then
        kill "$pid" 2>/dev/null || kill -9 "$pid" 2>/dev/null
        echo "[+] Data process killed"
    else
        echo "[!] No data process running"
    fi
}

get_repo_url() {
    local pid=$(get_data_pid)
    [[ -z "$pid" ]] && return 1
    cat /proc/$pid/environ 2>/dev/null | tr '\0' '\n' | grep "^REPO_URL=" | cut -d= -f2-
}

get_repo() {
    local pid=$(get_data_pid)
    [[ -z "$pid" ]] && return 1
    cat /proc/$pid/environ 2>/dev/null | tr '\0' '\n' | grep "^REPO=" | cut -d= -f2-
}

get_repo_folder() {
    local pid=$(get_data_pid)
    [[ -z "$pid" ]] && return 1
    cat /proc/$pid/environ 2>/dev/null | tr '\0' '\n' | grep "^REPO_FOLDER=" | cut -d= -f2-
}

get_ssh_key() {
    local pid=$(get_data_pid)
    [[ -z "$pid" ]] && return 1
    cat /proc/$pid/environ 2>/dev/null | tr '\0' '\n' | grep "^SSH_KEY_PATH=" | cut -d= -f2-
}

get_git_user() {
    local pid=$(get_data_pid)
    [[ -z "$pid" ]] && return 1
    cat /proc/$pid/environ 2>/dev/null | tr '\0' '\n' | grep "^GIT_USERNAME=" | cut -d= -f2-
}

get_git_email() {
    local pid=$(get_data_pid)
    [[ -z "$pid" ]] && return 1
    cat /proc/$pid/environ 2>/dev/null | tr '\0' '\n' | grep "^GIT_EMAIL=" | cut -d= -f2-
}

init_config() {
    if check_data_process; then
        echo "[+] Data process running"
        local repo=$(get_repo)
        [[ -n "$repo" ]] && echo "[+] REPO: $repo"
        return 0
    fi
    
    echo "[+] Starting configuration"
    
    read -p "Enter GitHub repo URL: " url
    [[ -z "$url" ]] && { echo "Error: URL required"; exit 1; }
    
    user_repo="$(basename "$(dirname "$url")")/$(basename "$url")"
    echo "$user_repo"
    
    REPO="$user_repo"
    REPO_FOLDER=$(basename "$REPO" .git)
    
    read -p "SSH key path [~/.ssh/id_rsa]: " ssh_input
    SSH_KEY_PATH="${ssh_input:-$HOME/.ssh/id_rsa}"
    
    read -p "Git username: " user_input
    if [[ -z "$user_input" ]]; then
        echo "Error: Git username required"
        exit 1
    fi
    GIT_USERNAME="$user_input"

    read -p "Git email: " email_input
    if [[ -z "$email_input" ]]; then
        echo "Error: Git email required"
        exit 1
    fi
    GIT_EMAIL="$email_input"

    start_data_process "$url" "$REPO" "$REPO_FOLDER" "$SSH_KEY_PATH" "$GIT_USERNAME" "$GIT_EMAIL"
}

# ============================================================================
# OPERATION FUNCTIONS - EXACT ORIGINAL STATEMENTS
# ============================================================================

func_do_acp() {
    echo "[+] Adding files <100MB"
    
    # Add everything first
    git add .
    
    # Remove large files from staging
    find . -size +100M -type f -not -path './.git/*' | while read -r file; do
        echo "REMOVING: $file (>100MB)"
        if git ls-files --cached "$file" >/dev/null 2>&1; then
            git reset HEAD -- "$file"  # For tracked files
        else
            git rm --cached "$file"  # For untracked staged files
        fi
    done
    
    git commit -m "$COMMIT_MSG"
    git push origin main
    exit 0
}

func_do_git_commit() {
    echo "[+] git commit -m \"$COMMIT_MSG\""; git commit -m "$COMMIT_MSG"
    exit 1
}

func_do_git_push() {
    echo "[+] git push"; git push
    exit 1
}

func_do_git_pull() {
    echo "[+] git pull"; git pull
    exit 1
}

func_do_git_clone() {
    PWD=$(pwd)

    # install essentials 
    sudo apt install -y build-essential libc6-dev # libstdc++-11-dev
    sudo apt install liblzma-dev

    # set permissions for others 
    # chmod +x $PWD/101-gitHubAuth-PullRepo.sh    # will pull the given repo link from the script 
    # chmod +x $PWD/102-restore_AOCC.sh           # will merge the p1 and p2 of AOCC to then install 
    # chmod +x $PWD/103-setup_AOCC_clang.sh       # will install the AOCC clang compiler
    # $PWD/101-gitHubAuth-PullRepo.sh
    #!/bin/bash
    # GitHub SSH auth and clone Repo

    # Start SSH agent
    eval "$(ssh-agent -s)"

    # Add SSH key
    ssh-add $(get_ssh_key)

    # Configure Git
    git config --global user.name "$(get_git_user)"

    # Test GitHub connection
    echo "Testing GitHub connection..."
    ssh -T git@github.com

    # Clone ModdedChampsim
    echo " >Cloning ModdedChampsim..."
    git clone git@github.com:$(get_repo)

    echo "  > ** Done! Repo cloned successfully. **"
    cd $(get_repo_folder)
    ls -la
}

func_do_aocc_restore() {
    set -e  # Exit on any error

    echo "=== AOCC Setup Script ==="

    # Step 1: Combine p1 and p2
    echo " >Step 1: Combining aocc-p1 and aocc-p2..."
    cat aocc-p1 aocc-p2 > aocc-compiler-5.0.0.tar

    # Verify combination
    echo "  >Verifying combined file..."
    if [ ! -f aocc-compiler-5.0.0.tar ]; then
        echo "  >ERROR: Failed to create combined tar file"
        exit 1
    fi

    SIZE=$(stat -c%s aocc-compiler-5.0.0.tar)
    echo "   > Combined file size: $SIZE bytes"

    echo "    > ** AOCC Archive restored successfully. **"
    # Verify
    which clang
    clang --version
}

func_do_aocc_setup() {
    # Extract AOCC
    echo " >Extracting AOCC..."
    tar -xf aocc-compiler-5.0.0.tar

    # Move to system location
    echo "  >Moving AOCC to /opt/..."
    sudo mv aocc-compiler-5.0.0 /opt/

    # Create symlinks to make AOCC default
    echo "   >Creating symlinks for AOCC..."
    sudo ln -sf /opt/aocc-compiler-5.0.0/bin/clang /usr/local/bin/clang
    sudo ln -sf /opt/aocc-compiler-5.0.0/bin/clang++ /usr/local/bin/clang++
    sudo ln -sf /opt/aocc-compiler-5.0.0/bin/flang /usr/local/bin/flang

    # Update alternatives system
    echo "    >Updating alternatives for AOCC..."
    sudo update-alternatives --install /usr/bin/clang clang /opt/aocc-compiler-5.0.0/bin/clang 100
    sudo update-alternatives --install /usr/bin/clang++ clang++ /opt/aocc-compiler-5.0.0/bin/clang++ 100

    # Set environment globally
    echo "     >Setting environment variables for AOCC..."
    echo 'export PATH=/opt/aocc-compiler-5.0.0/bin:$PATH' | sudo tee /etc/environment.d/aocc.conf
    echo 'source /opt/aocc-compiler-5.0.0/setenv_AOCC.sh' | sudo tee -a /etc/bash.bashrc

    # Source environment immediately
    export PATH=/opt/aocc-compiler-5.0.0/bin:$PATH

    # FINISHED INSTALL
    RECALL=$(pwd) 
    cd /opt/aocc-compiler-5.0.0/
    sudo ./install.sh
    cd $RECALL
    # AOCC environment without install.sh
    export PATH="/opt/aocc-compiler-5.0.0/bin:$PATH"
    export LD_LIBRARY_PATH="/opt/aocc-compiler-5.0.0/lib:$LD_LIBRARY_PATH"
    export CPLUS_INCLUDE_PATH="/opt/aocc-compiler-5.0.0/include/c++/v1:/usr/include/c++/11"
    sudo apt install -y libc++-17-dev libc++abi-17-dev
    # add to bash 
    echo 'export PATH="/opt/aocc-compiler-5.0.0/bin:$PATH"' | sudo tee -a /etc/bash.bashrc
    echo 'source /opt/aocc-compiler-5.0.0/setenv_AOCC.sh' | sudo tee -a /etc/bash.bashrc
    # Test ChampSim compilation
    clang++ --version
}

func_do_sim() {
    echo "Dependency install lzma.h"
    sudo apt install -y liblzma-dev
    echo "=== Testing ModdedChampsim ==="
    # cd to ModdedChampsim dir and give permissions: quick.sh buildPrefetcher.sh run_champsim.sh
    PWD=$(pwd)
    cd $PWD/$(get_repo_folder)/
    echo " >Setting permissions for scripts in ModdedChampsim..."
    chmod +x $PWD/quickSim/quick.sh
    chmod +x $PWD/buildPrefetcher.sh
    chmod +x $PWD/run_champsim.sh

    # cd to pin_champsim dir and give permissions: build_champsim.sh and run_4core.sh
    echo "  >Setting permissions for scripts in pin_champsim..."
    cd $PWD/pin_champsim/
    chmod +x $PWD/build_champsim.sh
    chmod +x $PWD/run_4core.sh
    # get back
    cd ..

    # now in ModdedChampsim dir, run the quick.sh script to test

    echo "   >Running quick.sh to test ModdedChampsim..."
    $PWD/$(get_repo_folder)/quickSim/quick.sh --cores 2 --debug 0 --fast --processes_num 1
}
func_do_env_install() {
    echo "[+] Installing dependencies..."
    
    # Update package list
    echo " >Updating apt..."
    sudo apt-get update
    
    # Try to install newer versions, fallback to older if fail
    echo "  >Installing build essentials and dependencies..."
    if ! sudo apt-get install -y \
        build-essential \
        g++-12 \
        libstdc++-12-dev \
        liblzma-dev \
        ccache \
        clang \
        gdb \
        gdbserver \
        linux-tools-common \
        linux-tools-generic \
        linux-tools-$(uname -r) 2>/dev/null; then
        
        echo "   >g++-12 not available, falling back to g++-11..."
        sudo apt-get install -y \
            build-essential \
            g++ \
            libstdc++-11-dev \
            liblzma-dev \
            ccache \
            clang \
            gdb \
            gdbserver \
            linux-tools-common \
            linux-tools-generic
    fi
    
    # Install Python and packages
    echo "   >Installing Python dependencies..."
    sudo apt install -y python3-pip
    pip3 install pandas --break-system-packages
    pip3 install openpyxl --break-system-packages
    
    # Install latest GCC
    echo "    >Installing latest GCC..."
    sudo apt install -y software-properties-common
    sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
    sudo apt-get update
    sudo apt-get install -y gcc-14 g++-14
    sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100
    sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100
    
    # Install LLVM 21.1.0 FULL PACKAGE
    echo "     >Installing LLVM 21.1.0 (FULL - 1GB download)..."
    LLVM_VERSION="21.1.0"
    LLVM_TAR="LLVM-${LLVM_VERSION}-Linux-X64.tar.xz"
    LLVM_URL="https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/${LLVM_TAR}"
    
    cd /tmp
    wget "$LLVM_URL"
    tar -xf "$LLVM_TAR"
    sudo mv "LLVM-${LLVM_VERSION}-Linux-X64" /opt/llvm-21.1.0
    
    # Create symlinks
    sudo ln -sf /opt/llvm-21.1.0/bin/clang /usr/local/bin/clang
    sudo ln -sf /opt/llvm-21.1.0/bin/clang++ /usr/local/bin/clang++
    sudo ln -sf /opt/llvm-21.1.0/bin/llvm-config /usr/local/bin/llvm-config
    
    # Add to PATH and LD_LIBRARY_PATH
    echo 'export PATH=/opt/llvm-21.1.0/bin:$PATH' | sudo tee /etc/profile.d/llvm-21.sh
    echo 'export LD_LIBRARY_PATH=/opt/llvm-21.1.0/lib:$LD_LIBRARY_PATH' | sudo tee -a /etc/profile.d/llvm-21.sh
    source /etc/profile.d/llvm-21.sh
    
    # Cleanup
    rm -f "/tmp/$LLVM_TAR"
    
    echo "      >Verifying installations..."
    gcc --version | head -n1
    g++ --version | head -n1
    clang --version | head -n1
    
    echo "       > ** All dependencies installed. LLVM 21.1.0 FULL at /opt/llvm-21.1.0 **"
    exit 0
}
func_do_shell_setup() {
    echo "[+] Installing zsh and zoxide..."
    
    # Install zsh
    echo " >Installing zsh..."
    sh -c "$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)" -y
    git clone https://github.com/zsh-users/zsh-autosuggestions ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-autosuggestions
    # replace plugins=(git) with plugins=(git zsh-autosuggestions) in .zshrc
    sed -i 's/^plugins=(git)/plugins=(git zsh-autosuggestions)/' ~/.zshrc
    # ${ZSH_CUSTOM:-${ZSH:-~/.oh-my-zsh}/custom}/plugins/zsh-autosuggestions
    # fpath+=${ZSH_CUSTOM:-${ZSH:-~/.oh-my-zsh}/custom}/plugins/zsh-autosuggestions/src
    # autoload -U compinit && compinit
    # source "$ZSH/oh-my-zsh.sh"
    
    # Install zoxide
    # echo "  >Installing zoxide..."
    # curl -sS https://raw.githubusercontent.com/ajeetdsouza/zoxide/main/install.sh | bash
    
    # Add to .bashrc to auto-launch zsh
    # echo "   >Configuring .bashrc to launch zsh..."
    # cat >> ~/.bashrc <<'EOF'

# # Auto-launch zsh
# if [[ -z "$ZSH_VERSION" ]]; then
#     exec zsh
# fi
# EOF

    # Create .zshrc
    echo "    >Configuring .zshrc..."
    cat > ~/.zshrc <<'EOF'
# zsh autocompletion
bindkey "^[[1;5C" forward-word      # Ctrl+Right
bindkey "^[[1;5D" backward-word     # Ctrl+Left
bindkey "^[[3;5~" kill-word          # Ctrl+Delete
bindkey "^H" backward-kill-word      # Ctrl+Backspace
alias ls='ls --color=auto'
alias ll='ls -alFh --color=auto'
alias grep='grep --color=auto'



EOF

    echo "     > ** Done. Run 'exec zsh' or restart terminal. **"
    exit 0
}

# ============================================================================
# MAIN
# ============================================================================

usage() {
  cat <<EOF
Usage: $0 [FLAGS]

Flags (MUST match exactly):
  -acp <msg>      Run add . commit -m <msg> and push
  -git            Run git pull
  -commit <msg>   Run git commit -m <msg>
  -push           Run git push
  -aocc           Run aocc restore
  -setup          Run aocc setup
  -all            Run full pipeline
  -sim            Run simulation only
  -kill           Kill data process
  -env_install    Install environment dependencies

Examples:
  $0 -git -commit "fix" -push
  $0 -all -commit "batch msg"
EOF
  exit 1
}

init_config

DO_GIT_CLONE=0
DO_GIT_COMMIT=0
DO_GIT_PUSH=0
DO_AOCC_RESTORE=0
DO_AOCC_SETUP=0
DO_ALL=0
DO_SIM=0
DO_ACP=0
DO_GIT_PULL=0
DO_KILL=0
DO_env_install=0
COMMIT_MSG=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -acp)
      [[ -z "${2:-}" || "$2" =~ ^- ]] && { echo "Error: -acp requires a message"; exit 1; }
      DO_ACP=1; DO_GIT_COMMIT=1; COMMIT_MSG="$2"; DO_GIT_PUSH=1; shift 2 ;;
    -git)
      DO_GIT_CLONE=1; shift ;;
    -pull) 
      DO_GIT_PULL=1; shift ;;
    -commit)
      [[ -z "${2:-}" || "$2" =~ ^- ]] && { echo "Error: -commit requires a message"; exit 1; }
      DO_GIT_COMMIT=1; COMMIT_MSG="$2"; shift 2 ;;
    -push)
      DO_GIT_PUSH=1; shift ;;
    -aocc)
      DO_AOCC_RESTORE=1; shift ;;
    -setup)
      DO_AOCC_SETUP=1; shift ;;
    -all)
      DO_ALL=1; shift ;;
    -sim)
      DO_SIM=1; shift ;;
    -kill)
      DO_KILL=1; shift ;;
    -env_install)
      DO_env_install=1; shift ;;
    -h|--help)
      usage ;;
    *)
      echo "Error: Unknown flag: $1"; usage ;;
  esac
done

if [[ $DO_KILL -eq 1 ]]; then
    kill_data_process
    exit 0
fi

if [[ $DO_ACP -eq 1 ]]; then
    func_do_acp
fi

if [[ $DO_GIT_COMMIT -eq 1 ]]; then
    func_do_git_commit
fi

if [[ $DO_GIT_PUSH -eq 1 ]]; then
    func_do_git_push
fi

if [[ $DO_GIT_PULL -eq 1 ]]; then
    func_do_git_pull
fi

if [[ $DO_ALL -eq 1 || $DO_GIT_CLONE -eq 1 ]]; then
    func_do_git_clone
fi

if [[ $DO_ALL -eq 1 || $DO_AOCC_RESTORE -eq 1 ]]; then
    func_do_aocc_restore
fi

if [[ $DO_ALL -eq 1 || $DO_AOCC_SETUP -eq 1 ]]; then
    func_do_aocc_setup
fi

if [[ $DO_ALL -eq 1 || $DO_SIM -eq 1 ]]; then
    func_do_sim
fi
if [[ $DO_env_install -eq 1 ]]; then
    func_do_env_install
    # func_do_shell_setup
    exit 0
fi