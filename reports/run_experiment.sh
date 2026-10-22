#!/bin/bash
set -u

DB="bench_db"
TIME=30
THREADS=2
BUFFERS_SIZES=("512MB" "1024MB")
CLIENTS=(1 4 10 20 50)
REPEATS=3

DATA_DIR="$HOME/postgres/data/debug"
LOG_DIR="$HOME/postgres/log/debug"
INSTALL_DIR="$HOME/postgres/install/debug"
PG_CTL="$INSTALL_DIR/bin/pg_ctl"
PGBENCH="$INSTALL_DIR/bin/pgbench"
PSQL="$INSTALL_DIR/bin/psql"

RESULT_FILE="$HOME/postgres/bench/ext_compare.csv"
TEMP_DIR="$HOME/postgres/bench/temp_compare"

mkdir -p "$TEMP_DIR"

restore_and_exit() {
    echo ""
    echo "!!! Прерывание. Восстанавливаю исходные настройки (128MB, без расширения)..."
    "$PSQL" -d postgres -c "ALTER SYSTEM SET shared_buffers = '128MB';" >/dev/null 2>&1
    "$PSQL" -d postgres -c "ALTER SYSTEM RESET shared_preload_libraries;" >/dev/null 2>&1
    "$PG_CTL" -D "$DATA_DIR" -l "$LOG_DIR/postgres.log" restart >/dev/null 2>&1
    echo "Готово. Выход."
    exit 1
}
trap restore_and_exit INT TERM

echo "pg_version,buffers,extension,clients,run,tps,latency_avg" > "$RESULT_FILE"

setup_server() {
    local size=$1
    local ext_status=$2
    
    echo "Настраиваем: shared_buffers=$size, pg_stat_statements=$ext_status"
    
    "$PSQL" -d postgres -c "ALTER SYSTEM SET shared_buffers = '$size';" >/dev/null
    
    if [ "$ext_status" = "on" ]; then
        "$PSQL" -d postgres -c "ALTER SYSTEM SET shared_preload_libraries = 'pg_stat_statements';" >/dev/null
    else
        "$PSQL" -d postgres -c "ALTER SYSTEM RESET shared_preload_libraries;" >/dev/null
    fi
    
    "$PG_CTL" -D "$DATA_DIR" -l "$LOG_DIR/postgres.log" restart >/dev/null
    sleep 2
    
    local applied_buf=$("$PSQL" -d postgres -Atc "SHOW shared_buffers;" | tr -d ' ')
    local applied_lib=$("$PSQL" -d postgres -Atc "SHOW shared_preload_libraries;" | tr -d ' ')
    
    echo "  Применено: shared_buffers=$applied_buf, preload='$applied_lib'"
    if [ "$applied_buf" != "$size" ]; then
        echo "!!! Ошибка: shared_buffers не применился!"
        restore_and_exit
    fi
}

run_once() {
    local clients=$1
    local temp_log="$TEMP_DIR/bench_c${clients}_$$.log"
    "$PGBENCH" -c "$clients" -j "$THREADS" -T "$TIME" -d "$DB" > "$temp_log" 2>&1
    
    local tps=$(grep -E "^tps = " "$temp_log" | head -1 | awk '{print $3}')
    local lat=$(grep -E "^latency average = " "$temp_log" | head -1 | awk '{print $4}')
    
    if [ -z "$tps" ] || [ -z "$lat" ]; then
        echo "ERROR ERROR"
    else
        echo "$tps $lat"
    fi
    rm -f "$temp_log"
}

PG_VERSION=$("$PSQL" -d postgres -Atc "SHOW server_version;")

echo "Начинаем сравнительное тестирование..."

for buffers in "${BUFFERS_SIZES[@]}"; do
    for ext in "off" "on"; do
        setup_server "$buffers" "$ext"
        
        for clients in "${CLIENTS[@]}"; do
            for run in $(seq 1 "$REPEATS"); do
                echo -n "buffers=$buffers ext=$ext clients=$clients run=$run ... "
                read -r tps lat <<< "$(run_once "$clients")"
                echo "TPS=$tps latency=$lat"
                echo "$PG_VERSION,$buffers,$ext,$clients,$run,$tps,$lat" >> "$RESULT_FILE"
            done
        done
    done
done

echo ""
echo "Все тесты завершены!"
restore_and_exit