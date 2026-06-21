#!/usr/bin/env bash
###############################################################################
# 01-setup-env.sh — Install Ollama + Claude Code + Agent Teams (idempotent)
###############################################################################
set -euo pipefail
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
log()  { echo -e "${GREEN}[✓]${NC} $*"; }
warn() { echo -e "${YELLOW}[!]${NC} $*"; }
err()  { echo -e "${RED}[✗]${NC} $*" >&2; }
info() { echo -e "${CYAN}[i]${NC} $*"; }

install_ollama() {
    if command -v ollama &>/dev/null; then
        local ver=$(ollama --version 2>&1 | grep -oP '[\d]+\.[\d]+\.[\d]+' | head -1 || echo "unknown")
        log "Ollama installed (${ver})"
        local minor=$(echo "$ver" | cut -d. -f2)
        if [[ "${minor:-0}" -lt 14 ]]; then
            warn "Ollama <0.14.0 — Anthropic API compat missing. Upgrade recommended."
            read -rp "Upgrade? [y/N]: " ans
            [[ "${ans,,}" == "y" ]] && curl -fsSL https://ollama.com/install.sh | sh
        fi
    else
        info "Installing Ollama..."
        curl -fsSL https://ollama.com/install.sh | sh
        log "Ollama installed"
    fi
    if ! curl -sf http://localhost:11434/api/version &>/dev/null; then
        info "Starting Ollama..."
        if command -v systemctl &>/dev/null; then
            sudo systemctl start ollama 2>/dev/null || nohup ollama serve &>/dev/null &
        else
            nohup ollama serve &>/dev/null &
        fi
        sleep 3
    fi
    curl -sf http://localhost:11434/api/version &>/dev/null && log "Ollama running" || warn "Start manually: ollama serve"
}

install_node() {
    if command -v node &>/dev/null; then
        log "Node.js $(node --version)"
        local major=$(node --version | sed 's/v//' | cut -d. -f1)
        [[ "$major" -lt 18 ]] && warn "Node <18, Claude Code needs >=18"
    else
        info "Installing Node.js 20.x..."
        curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash - && sudo apt-get install -y nodejs
        log "Node.js installed"
    fi
}

install_claude_code() {
    if command -v claude &>/dev/null; then
        log "Claude Code installed"
    else
        info "Installing Claude Code..."
        curl -fsSL https://claude.ai/install.sh -o /tmp/cc.sh 2>/dev/null && bash /tmp/cc.sh && rm -f /tmp/cc.sh \
            || npm install -g @anthropic-ai/claude-code 2>/dev/null \
            || sudo npm install -g @anthropic-ai/claude-code
        command -v claude &>/dev/null && log "Claude Code installed" || { err "Install failed"; exit 1; }
    fi
}

setup_agent_teams() {
    local settings_dir="${HOME}/.claude"
    local settings_file="${settings_dir}/settings.json"
    mkdir -p "$settings_dir"

    if [[ -f "$settings_file" ]] && grep -q "CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS" "$settings_file" 2>/dev/null; then
        log "Agent Teams already configured"
    else
        info "Enabling Agent Teams in Claude Code settings..."
        if [[ -f "$settings_file" ]]; then
            # Merge into existing settings
            python3 -c "
import json, sys
with open('$settings_file') as f:
    cfg = json.load(f)
cfg.setdefault('env', {})
cfg['env']['CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS'] = '1'
with open('$settings_file', 'w') as f:
    json.dump(cfg, f, indent=2)
" 2>/dev/null || {
                warn "Could not merge settings, creating new"
                cat > "$settings_file" << 'EOF'
{
  "env": {
    "CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS": "1"
  }
}
EOF
            }
        else
            cat > "$settings_file" << 'EOF'
{
  "env": {
    "CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS": "1"
  }
}
EOF
        fi
        log "Agent Teams enabled"
    fi
}

check_gpu() {
    info "=== GPU Detection ==="
    if command -v nvidia-smi &>/dev/null; then
        nvidia-smi --query-gpu=name,memory.total --format=csv,noheader 2>/dev/null | while read -r l; do log "GPU: $l"; done
        local total=$(nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>/dev/null | awk '{s+=$1}END{print s}')
        log "Total VRAM: ${total} MiB"
        if [[ "$total" -ge 300000 ]]; then info "Tier: KING (4xA100)";
        elif [[ "$total" -ge 8000 ]]; then info "Tier: LOCAL (single GPU)";
        else info "Tier: MINIMAL"; fi
    else
        warn "nvidia-smi not found"
    fi
}

main() {
    echo "=============================================="
    echo "  DSE v2 Environment Setup"
    echo "=============================================="
    install_ollama; echo ""
    install_node; echo ""
    install_claude_code; echo ""
    setup_agent_teams; echo ""
    check_gpu
    echo ""
    log "Done. Next: ./02-pull-models.sh"
}
main "$@"
