#!/bin/bash

# IKOS Daemon Management Utility
# Command-line interface for daemon management system

DAEMON_CONFIG_DIR="/etc/ikos/daemons"
DAEMON_PID_DIR="/var/run/ikos"
DAEMON_LOG_DIR="/var/log/ikos"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root"
        exit 1
    fi
}

# Create necessary directories
setup_directories() {
    mkdir -p "$DAEMON_CONFIG_DIR"
    mkdir -p "$DAEMON_PID_DIR"
    mkdir -p "$DAEMON_LOG_DIR"
    
    # Set proper permissions
    chmod 755 "$DAEMON_CONFIG_DIR"
    chmod 755 "$DAEMON_PID_DIR"
    chmod 755 "$DAEMON_LOG_DIR"
}

# List all configured daemons
list_daemons() {
    log_info "Configured daemons:"
    echo
    
    if [ ! -d "$DAEMON_CONFIG_DIR" ]; then
        log_warning "No daemon configuration directory found"
        return
    fi
    
    printf "%-20s %-10s %-30s\n" "NAME" "STATUS" "DESCRIPTION"
    printf "%-20s %-10s %-30s\n" "----" "------" "-----------"
    
    for config_file in "$DAEMON_CONFIG_DIR"/*.conf; do
        if [ -f "$config_file" ]; then
            daemon_name=$(basename "$config_file" .conf)
            
            # Check if daemon is running
            pid_file="$DAEMON_PID_DIR/$daemon_name.pid"
            if [ -f "$pid_file" ]; then
                pid=$(cat "$pid_file")
                if kill -0 "$pid" 2>/dev/null; then
                    status="${GREEN}RUNNING${NC}"
                else
                    status="${RED}STOPPED${NC}"
                fi
            else
                status="${RED}STOPPED${NC}"
            fi
            
            # Get description from config file
            description=$(grep "^description" "$config_file" | cut -d'=' -f2 | xargs)
            if [ -z "$description" ]; then
                description="No description"
            fi
            
            printf "%-20s %-20s %-30s\n" "$daemon_name" "$status" "$description"
        fi
    done
}

# Show daemon status
daemon_status() {
    local daemon_name="$1"
    
    if [ -z "$daemon_name" ]; then
        log_error "Daemon name required"
        exit 1
    fi
    
    config_file="$DAEMON_CONFIG_DIR/$daemon_name.conf"
    if [ ! -f "$config_file" ]; then
        log_error "Daemon '$daemon_name' not configured"
        exit 1
    fi
    
    pid_file="$DAEMON_PID_DIR/$daemon_name.pid"
    
    log_info "Status for daemon '$daemon_name':"
    echo
    
    # Basic information
    description=$(grep "^description" "$config_file" | cut -d'=' -f2 | xargs)
    executable=$(grep "^executable" "$config_file" | cut -d'=' -f2 | xargs)
    
    echo "Description: $description"
    echo "Executable:  $executable"
    
    # Running status
    if [ -f "$pid_file" ]; then
        pid=$(cat "$pid_file")
        if kill -0 "$pid" 2>/dev/null; then
            echo -e "Status:      ${GREEN}RUNNING${NC} (PID: $pid)"
            
            # Show resource usage
            if command -v ps &> /dev/null; then
                cpu_mem=$(ps -p "$pid" -o %cpu,%mem --no-headers)
                echo "Resources:   CPU: $(echo $cpu_mem | awk '{print $1}')%, Memory: $(echo $cpu_mem | awk '{print $2}')%"
            fi
            
            # Show uptime
            if [ -f "/proc/$pid/stat" ]; then
                start_time=$(stat -c %Y "/proc/$pid")
                current_time=$(date +%s)
                uptime=$((current_time - start_time))
                echo "Uptime:      $(($uptime / 3600))h $(($uptime % 3600 / 60))m $(($uptime % 60))s"
            fi
        else
            echo -e "Status:      ${RED}STOPPED${NC} (stale PID file)"
        fi
    else
        echo -e "Status:      ${RED}STOPPED${NC}"
    fi
    
    # Log file information
    log_file="$DAEMON_LOG_DIR/$daemon_name.log"
    if [ -f "$log_file" ]; then
        log_size=$(stat -c%s "$log_file")
        echo "Log file:    $log_file ($(($log_size / 1024)) KB)"
    fi
}

# Start a daemon
start_daemon() {
    local daemon_name="$1"
    
    if [ -z "$daemon_name" ]; then
        log_error "Daemon name required"
        exit 1
    fi
    
    config_file="$DAEMON_CONFIG_DIR/$daemon_name.conf"
    if [ ! -f "$config_file" ]; then
        log_error "Daemon '$daemon_name' not configured"
        exit 1
    fi
    
    pid_file="$DAEMON_PID_DIR/$daemon_name.pid"
    
    # Check if already running
    if [ -f "$pid_file" ]; then
        pid=$(cat "$pid_file")
        if kill -0 "$pid" 2>/dev/null; then
            log_warning "Daemon '$daemon_name' is already running (PID: $pid)"
            return
        else
            log_info "Removing stale PID file"
            rm -f "$pid_file"
        fi
    fi
    
    # Read configuration
    executable=$(grep "^executable" "$config_file" | cut -d'=' -f2 | xargs)
    working_dir=$(grep "^working_directory" "$config_file" | cut -d'=' -f2 | xargs)
    
    if [ -z "$executable" ]; then
        log_error "No executable specified in configuration"
        exit 1
    fi
    
    if [ ! -x "$executable" ]; then
        log_error "Executable '$executable' not found or not executable"
        exit 1
    fi
    
    # Prepare arguments
    args=""
    for i in {0..9}; do
        arg=$(grep "^arg$i" "$config_file" | cut -d'=' -f2 | xargs)
        if [ -n "$arg" ]; then
            args="$args \"$arg\""
        fi
    done
    
    # Prepare environment
    env_vars=""
    for i in {0..9}; do
        env_var=$(grep "^env$i" "$config_file" | cut -d'=' -f2 | xargs)
        if [ -n "$env_var" ]; then
            env_vars="$env_vars $env_var"
        fi
    done
    
    # Set working directory
    if [ -n "$working_dir" ] && [ -d "$working_dir" ]; then
        cd "$working_dir"
    fi
    
    log_info "Starting daemon '$daemon_name'..."
    
    # Start the daemon
    log_file="$DAEMON_LOG_DIR/$daemon_name.log"
    
    if [ -n "$env_vars" ]; then
        env $env_vars nohup $executable $args > "$log_file" 2>&1 &
    else
        nohup $executable $args > "$log_file" 2>&1 &
    fi
    
    daemon_pid=$!
    
    # Save PID
    echo "$daemon_pid" > "$pid_file"
    
    # Wait a moment and check if it's still running
    sleep 1
    if kill -0 "$daemon_pid" 2>/dev/null; then
        log_success "Daemon '$daemon_name' started successfully (PID: $daemon_pid)"
    else
        log_error "Daemon '$daemon_name' failed to start"
        rm -f "$pid_file"
        exit 1
    fi
}

# Stop a daemon
stop_daemon() {
    local daemon_name="$1"
    
    if [ -z "$daemon_name" ]; then
        log_error "Daemon name required"
        exit 1
    fi
    
    pid_file="$DAEMON_PID_DIR/$daemon_name.pid"
    
    if [ ! -f "$pid_file" ]; then
        log_warning "Daemon '$daemon_name' is not running"
        return
    fi
    
    pid=$(cat "$pid_file")
    
    if ! kill -0 "$pid" 2>/dev/null; then
        log_warning "Daemon '$daemon_name' is not running (removing stale PID file)"
        rm -f "$pid_file"
        return
    fi
    
    log_info "Stopping daemon '$daemon_name' (PID: $pid)..."
    
    # Try graceful shutdown first
    kill -TERM "$pid"
    
    # Wait for graceful shutdown
    for i in {1..10}; do
        if ! kill -0 "$pid" 2>/dev/null; then
            break
        fi
        sleep 1
    done
    
    # Force kill if still running
    if kill -0 "$pid" 2>/dev/null; then
        log_warning "Daemon not responding to SIGTERM, sending SIGKILL"
        kill -KILL "$pid"
        sleep 2
    fi
    
    # Check if stopped
    if kill -0 "$pid" 2>/dev/null; then
        log_error "Failed to stop daemon '$daemon_name'"
        exit 1
    else
        log_success "Daemon '$daemon_name' stopped successfully"
        rm -f "$pid_file"
    fi
}

# Restart a daemon
restart_daemon() {
    local daemon_name="$1"
    
    log_info "Restarting daemon '$daemon_name'..."
    stop_daemon "$daemon_name"
    sleep 1
    start_daemon "$daemon_name"
}

# Show daemon logs
show_logs() {
    local daemon_name="$1"
    local lines="${2:-50}"
    
    if [ -z "$daemon_name" ]; then
        log_error "Daemon name required"
        exit 1
    fi
    
    log_file="$DAEMON_LOG_DIR/$daemon_name.log"
    
    if [ ! -f "$log_file" ]; then
        log_warning "No log file found for daemon '$daemon_name'"
        return
    fi
    
    log_info "Last $lines lines of log for daemon '$daemon_name':"
    echo
    tail -n "$lines" "$log_file"
}

# Create daemon configuration template
create_config() {
    local daemon_name="$1"
    local executable="$2"
    
    if [ -z "$daemon_name" ] || [ -z "$executable" ]; then
        log_error "Usage: $0 create <daemon_name> <executable_path>"
        exit 1
    fi
    
    config_file="$DAEMON_CONFIG_DIR/$daemon_name.conf"
    
    if [ -f "$config_file" ]; then
        log_error "Configuration for daemon '$daemon_name' already exists"
        exit 1
    fi
    
    if [ ! -x "$executable" ]; then
        log_error "Executable '$executable' not found or not executable"
        exit 1
    fi
    
    log_info "Creating configuration for daemon '$daemon_name'..."
    
    cat > "$config_file" << EOF
# IKOS Daemon Configuration: $daemon_name
# Generated automatically

[daemon]
description = $daemon_name daemon
executable = $executable
# working_directory = /
# arg0 = first_argument
# arg1 = second_argument
# env0 = VARIABLE=value

[restart]
auto_restart = true
max_attempts = 3
delay = 5

[timeouts]
startup = 30
shutdown = 10

[logging]
level = info
syslog = true
file = true
file_path = $DAEMON_LOG_DIR/$daemon_name.log

[security]
user = 0
group = 0

[limits]
cpu_percent = 0
memory_mb = 0
file_descriptors = 1024
core_dump = false
priority = 0
EOF
    
    chmod 644 "$config_file"
    log_success "Configuration created: $config_file"
    log_info "Edit the configuration file and then start the daemon with: $0 start $daemon_name"
}

# Show usage
usage() {
    echo "IKOS Daemon Management Utility"
    echo
    echo "Usage: $0 <command> [arguments]"
    echo
    echo "Commands:"
    echo "  list                    List all configured daemons"
    echo "  status <daemon>         Show status of a daemon"
    echo "  start <daemon>          Start a daemon"
    echo "  stop <daemon>           Stop a daemon"
    echo "  restart <daemon>        Restart a daemon"
    echo "  logs <daemon> [lines]   Show daemon logs (default: 50 lines)"
    echo "  create <daemon> <exe>   Create daemon configuration template"
    echo "  help                    Show this help message"
    echo
    echo "Examples:"
    echo "  $0 list"
    echo "  $0 status web-server"
    echo "  $0 start web-server"
    echo "  $0 logs web-server 100"
    echo "  $0 create my-daemon /usr/bin/my-program"
}

# Main command processing
main() {
    if [ $# -eq 0 ]; then
        usage
        exit 1
    fi
    
    command="$1"
    shift
    
    case "$command" in
        "list")
            setup_directories
            list_daemons
            ;;
        "status")
            setup_directories
            daemon_status "$1"
            ;;
        "start")
            check_root
            setup_directories
            start_daemon "$1"
            ;;
        "stop")
            check_root
            setup_directories
            stop_daemon "$1"
            ;;
        "restart")
            check_root
            setup_directories
            restart_daemon "$1"
            ;;
        "logs")
            setup_directories
            show_logs "$1" "$2"
            ;;
        "create")
            check_root
            setup_directories
            create_config "$1" "$2"
            ;;
        "help"|"-h"|"--help")
            usage
            ;;
        *)
            log_error "Unknown command: $command"
            usage
            exit 1
            ;;
    esac
}

# Run main function
main "$@"
