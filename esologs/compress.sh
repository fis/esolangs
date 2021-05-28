#! /bin/bash -e

# batch job to Brotli-compress old log files

umask 0077

cutoff="$(expr $(date +%s) - 172800)"

find /home/esowiki/logs /home/esowiki/logs-libera -name '*.pb' | while read logfile; do
    if [[ "$logfile" =~ /([0-9]+)/([0-9]+)/([0-9]+)\.pb$ ]]; then
        logtime="$(TZ=UTC0 date --date="${BASH_REMATCH[1]}-${BASH_REMATCH[2]}-${BASH_REMATCH[3]}" +%s)"
        if (( logtime < cutoff )); then
            brotli --quality=11 < "$logfile" > "${logfile}.br"
            if diff -q "$logfile" <(brotli --decompress < "${logfile}.br"); then
                rm "$logfile"
            else
                echo "compressed logfile differs: $logfile" >&2
                exit 1
            fi
        fi
    else
        echo "invalid logfile path: $logfile" >&2
        exit 1
    fi
done
