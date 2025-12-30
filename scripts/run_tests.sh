#!/bin/bash
# run_tests.sh - Run timing measurement tests

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."
PAGEWALK_TEST="$PROJECT_DIR/userspace/pagewalk_test"
LATENCY_TEST="$PROJECT_DIR/userspace/int_latency_test"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================="
echo "  Timing Measurement Test Suite"
echo "========================================="
echo ""

case "${1:-all}" in
    pagewalk)
        echo -e "${GREEN}Running pagewalk timing test...${NC}"
        if [ ! -e /dev/pagewalk_timer ]; then
            echo -e "${RED}Error: /dev/pagewalk_timer not found${NC}"
            echo "Load module: insmod pagewalk_driver.ko"
            exit 1
        fi
        "$PAGEWALK_TEST" "${@:2}"
        ;;
    latency)
        SAMPLES="${2:-10}"
        echo -e "${GREEN}Running interrupt latency test ($SAMPLES samples)...${NC}"
        if [ ! -e /dev/int_latency ]; then
            echo -e "${RED}Error: /dev/int_latency not found${NC}"
            echo "Load module: insmod int_latency_driver.ko"
            exit 1
        fi
        "$LATENCY_TEST" "$SAMPLES"
        ;;
    all)
        echo -e "${GREEN}Running all tests...${NC}"
        echo ""
        
        if [ -e /dev/pagewalk_timer ]; then
            echo "=== Pagewalk Test ==="
            "$PAGEWALK_TEST"
            echo ""
        else
            echo -e "${YELLOW}Skipping pagewalk (module not loaded)${NC}"
        fi
        
        if [ -e /dev/int_latency ]; then
            echo "=== Interrupt Latency Test ==="
            "$LATENCY_TEST" 10
        else
            echo -e "${YELLOW}Skipping latency (module not loaded)${NC}"
        fi
        ;;
    help|--help|-h)
        echo "Usage: $0 [test_type] [options]"
        echo ""
        echo "Test types:"
        echo "  pagewalk [addr]  - Run pagewalk timing test"
        echo "  latency [n]      - Run interrupt latency test with n samples (default: 10)"
        echo "  all              - Run all tests (default)"
        echo "  help             - Show this help"
        echo ""
        echo "Examples:"
        echo "  $0                     # Run all tests"
        echo "  $0 pagewalk            # Run pagewalk test"
        echo "  $0 latency 100         # Run latency test with 100 samples"
        ;;
    *)
        echo -e "${RED}Unknown test type: $1${NC}"
        echo "Run '$0 help' for usage"
        exit 1
        ;;
esac

echo ""
echo -e "${GREEN}Done${NC}"
