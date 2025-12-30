#!/bin/bash
# run_tests.sh - Run pagewalk timing tests

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."
TEST_APP="$PROJECT_DIR/userspace/pagewalk_test"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================="
echo "  Pagewalk Timing Test Suite"
echo "========================================="
echo ""

# Check if test app exists
if [ ! -x "$TEST_APP" ]; then
    echo -e "${YELLOW}Test application not found. Building...${NC}"
    make -C "$PROJECT_DIR" userspace
    if [ $? -ne 0 ]; then
        echo -e "${RED}Failed to build test application${NC}"
        exit 1
    fi
fi

# Check if device exists
if [ ! -e /dev/pagewalk_timer ]; then
    echo -e "${RED}Error: /dev/pagewalk_timer not found${NC}"
    echo ""
    echo "Please load the kernel module first:"
    echo "  sudo $SCRIPT_DIR/load_module.sh"
    echo "  or"
    echo "  sudo make -C $PROJECT_DIR load"
    exit 1
fi

# Run tests based on arguments
case "${1:-all}" in
    sync)
        echo -e "${GREEN}Running synchronous pagewalk test...${NC}"
        "$TEST_APP"
        ;;
    irq)
        echo -e "${GREEN}Running interrupt latency test...${NC}"
        "$TEST_APP" -i
        ;;
    stats)
        SAMPLES="${2:-100}"
        echo -e "${GREEN}Running statistics test ($SAMPLES samples)...${NC}"
        "$TEST_APP" -s -n "$SAMPLES"
        ;;
    invalid)
        echo -e "${GREEN}Running invalid address test...${NC}"
        "$TEST_APP" -v
        ;;
    all)
        echo -e "${GREEN}Running all tests...${NC}"
        "$TEST_APP" -A
        ;;
    pid)
        if [ -z "$2" ]; then
            echo -e "${RED}Usage: $0 pid <PID> [address_hex]${NC}"
            exit 1
        fi
        TARGET_PID="$2"
        ADDR="${3:-}"
        echo -e "${GREEN}Testing PID $TARGET_PID...${NC}"
        if [ -n "$ADDR" ]; then
            "$TEST_APP" -p "$TARGET_PID" -a "$ADDR" -A
        else
            "$TEST_APP" -p "$TARGET_PID" -A
        fi
        ;;
    benchmark)
        SAMPLES="${2:-1000}"
        echo -e "${GREEN}Running benchmark ($SAMPLES samples)...${NC}"
        echo ""
        "$TEST_APP" -s -n "$SAMPLES"
        ;;
    help|--help|-h)
        echo "Usage: $0 [test_type] [options]"
        echo ""
        echo "Test types:"
        echo "  sync      - Run synchronous pagewalk test"
        echo "  irq       - Run interrupt latency test"
        echo "  stats [n] - Run statistics test with n samples (default: 100)"
        echo "  invalid   - Run invalid address test"
        echo "  all       - Run all tests (default)"
        echo "  pid <PID> [addr] - Test specific process"
        echo "  benchmark [n] - Run benchmark with n samples (default: 1000)"
        echo "  help      - Show this help"
        echo ""
        echo "Examples:"
        echo "  $0                    # Run all tests"
        echo "  $0 stats 500          # Collect 500 samples"
        echo "  $0 pid 1234           # Test process 1234"
        echo "  $0 pid 1234 0x7fff000 # Test specific address in process"
        ;;
    *)
        echo -e "${RED}Unknown test type: $1${NC}"
        echo "Run '$0 help' for usage"
        exit 1
        ;;
esac

echo ""
echo -e "${GREEN}Tests completed${NC}"
