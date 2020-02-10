#!/bin/bash -e

declare -A JOBS

run_job()
{
    eval $1 & JOBS[$!]="$1"
}

wait_jobs()
{
    local cmd
    local status=0
    for pid in ${!JOBS[@]}; do
        cmd=${JOBS[${pid}]}
        wait ${pid} ; JOBS[${pid}]=$?
        if [[ ${JOBS[${pid}]} -ne 0 ]]; then
            status=${JOBS[${pid}]}
            echo -e "[${pid}] Exited with status: ${status}\n${cmd}"
        fi
    done
    return ${status}
}
