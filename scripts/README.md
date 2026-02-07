# Windows Setup Guide - Environment Variables

This guide is specifically for Windows users working with the COMP6216 Swaying Swarms project.

## 🪟 Windows Shell Environments

You have **two shell options** on Windows:

1. **Git Bash (MINGW64)** - Unix-like bash shell
2. **PowerShell** - Windows native shell

**Both are fully supported!** Choose based on your preference.

---

## 🔧 Quick Setup

### Git Bash (MINGW64) Instructions

```bash
# 1. Open Git Bash terminal
# 2. Navigate to project
cd ~/OneDrive\ -\ University\ of\ Southampton/CS\ resources/Year\ 4/Semester\ II/COMP6216\ SMCS/Coursework/GCW/COMP6216-Swaying-Swarms

# 3. Load environment variables
source scripts/load-env.sh

# 4. Verify
env | grep CLAUDE_CODE
```

**Expected output:**
```
CLAUDE_CODE_SUBAGENT_MODEL=sonnet
CLAUDE_CODE_EFFORT_LEVEL=medium
```

### PowerShell Instructions

```powershell
# 1. Open PowerShell terminal
# 2. Navigate to project
cd "C:\Users\totob\OneDrive - University of Southampton\CS resources\Year 4\Semester II\COMP6216 SMCS\Coursework\GCW\COMP6216-Swaying-Swarms"

# 3. Load environment variables
. .\scripts\load-env.ps1

# 4. Verify
Get-ChildItem Env: | Where-Object Name -like 'CLAUDE_CODE*'
```

**Expected output:**
```
Name                           Value
----                           -----
CLAUDE_CODE_SUBAGENT_MODEL     sonnet
CLAUDE_CODE_EFFORT_LEVEL       medium
```

---

## 📝 Command Reference

### Git Bash vs PowerShell

| Action | Git Bash | PowerShell |
|--------|----------|------------|
| **Load variables** | `source scripts/load-env.sh` | `. .\scripts\load-env.ps1` |
| **View config** | `./scripts/load-env.sh` | `.\scripts\load-env.ps1` |
| **Update variable** | `./scripts/update-env.sh VAR value` | `.\scripts\update-env.ps1 VAR value` |
| **Check variables** | `env \| grep CLAUDE_CODE` | `Get-ChildItem Env: \| Where-Object Name -like 'CLAUDE_CODE*'` |
| **Check single var** | `echo $CLAUDE_CODE_SUBAGENT_MODEL` | `$env:CLAUDE_CODE_SUBAGENT_MODEL` |

---

## 🔄 Updating Configuration

### Git Bash

```bash
# Method 1: Quick update
./scripts/update-env.sh CLAUDE_CODE_SUBAGENT_MODEL opus
source scripts/load-env.sh

# Method 2: Manual edit
nano .env  # or: vim .env, code .env
source scripts/load-env.sh
```

### PowerShell

```powershell
# Method 1: Quick update
.\scripts\update-env.ps1 CLAUDE_CODE_SUBAGENT_MODEL opus
. .\scripts\load-env.ps1

# Method 2: Manual edit
notepad .env
. .\scripts\load-env.ps1
```

---

## 🚨 Common Issues & Solutions

### Issue 1: "source: command not found" in PowerShell

**Problem:** You tried to use `source` in PowerShell
```powershell
# ❌ WRONG
source scripts/load-env.sh
```

**Solution:** Use dot-sourcing with PowerShell script
```powershell
# ✓ CORRECT
. .\scripts\load-env.ps1
```

### Issue 2: Variables not persisting in Git Bash

**Problem:** You ran the script without `source`
```bash
# ❌ WRONG
./scripts/load-env.sh
```

**Solution:** Use `source` command
```bash
# ✓ CORRECT
source scripts/load-env.sh
```

### Issue 3: "cannot be loaded because running scripts is disabled" in PowerShell

**Problem:** PowerShell execution policy blocks scripts

**Solution:** Enable scripts for current user (one-time)
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

Then try loading again:
```powershell
. .\scripts\load-env.ps1
```

### Issue 4: Variables only show `CLAUDE_CODE_SSE_PORT`

**Problem:** You haven't loaded the environment variables yet

**Solution:** Load them using the appropriate command for your shell

**Git Bash:**
```bash
source scripts/load-env.sh
```

**PowerShell:**
```powershell
. .\scripts\load-env.ps1
```

### Issue 5: Path with spaces causing issues

**Problem:** Your path has spaces (e.g., "University of Southampton")

**Solution for Git Bash:** Use backslash escaping or quotes
```bash
cd ~/OneDrive\ -\ University\ of\ Southampton/.../COMP6216-Swaying-Swarms
# or
cd "~/OneDrive - University of Southampton/.../COMP6216-Swaying-Swarms"
```

**Solution for PowerShell:** Use quotes
```powershell
cd "C:\Users\totob\OneDrive - University of Southampton\...\COMP6216-Swaying-Swarms"
```

---

## 🤖 Auto-Load (Optional)

### Git Bash Auto-Load

Add to `~/.bashrc`:
```bash
# Auto-load COMP6216 environment
PROJECT_DIR="$HOME/OneDrive - University of Southampton/CS resources/Year 4/Semester II/COMP6216 SMCS/Coursework/GCW/COMP6216-Swaying-Swarms"
if [ -f "$PROJECT_DIR/scripts/load-env.sh" ]; then
    source "$PROJECT_DIR/scripts/load-env.sh"
fi
```

Then reload:
```bash
source ~/.bashrc
```

### PowerShell Auto-Load

Add to `$PROFILE` (create if doesn't exist):
```powershell
# To find your profile location
echo $PROFILE

# To create/edit it
notepad $PROFILE

# Add this content:
$ProjectDir = "C:\Users\totob\OneDrive - University of Southampton\CS resources\Year 4\Semester II\COMP6216 SMCS\Coursework\GCW\COMP6216-Swaying-Swarms"
$LoadEnvScript = Join-Path $ProjectDir "scripts\load-env.ps1"
if (Test-Path $LoadEnvScript) {
    . $LoadEnvScript
}
```

Then reload:
```powershell
. $PROFILE
```

---

## 📂 File Structure

```
COMP6216-Swaying-Swarms/
├── .env                    # Your config (gitignored)
├── .env.example            # Template (committed)
├── scripts/
│   ├── load-env.sh        # For Git Bash
│   ├── load-env.ps1       # For PowerShell
│   ├── update-env.sh      # For Git Bash
│   └── update-env.ps1     # For PowerShell
└── ...
```

---

## ✅ Verification Checklist

After setup, verify everything works:

### In Git Bash:
```bash
cd COMP6216-Swaying-Swarms
source scripts/load-env.sh
env | grep CLAUDE_CODE
# Should show 2 variables
```

### In PowerShell:
```powershell
cd COMP6216-Swaying-Swarms
. .\scripts\load-env.ps1
Get-ChildItem Env: | Where-Object Name -like 'CLAUDE_CODE*'
# Should show 2 variables
```

---

## 💡 Pro Tips

1. **Choose one shell and stick with it** for consistency
2. **Git Bash is recommended** if you plan to follow Linux-style master_plan instructions
3. **PowerShell is fine too** - all scripts have PowerShell versions
4. **VS Code terminal** can use either shell (change in settings)
5. **Always reload after editing .env** - changes don't auto-apply

---

## 🔗 Next Steps

After verifying environment variables work:

1. ✅ Environment setup complete
2. ➡️ Continue with Phase 2 of master_plan.md (Repository Scaffolding)
3. 🚀 Start building the simulation!

---

## 📞 Troubleshooting Help

If you're still having issues:

1. Check which shell you're in:
   - Git Bash shows: `totob@ALBERTOSMACHINE MINGW64`
   - PowerShell shows: `PS C:\Users\...`

2. Use the correct script for your shell:
   - Git Bash: `.sh` files
   - PowerShell: `.ps1` files

3. Make sure you're in the project directory:
   ```bash
   # Git Bash
   pwd
   
   # PowerShell
   Get-Location
   ```

4. Check if .env exists:
   ```bash
   # Git Bash
   ls -la .env
   
   # PowerShell
   Get-Item .env
   ```