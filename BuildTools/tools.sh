#!/bin/bash -e

declare -a JOBS

function run_job()
{
    eval $1 & JOBS[$!]="$1"
}

function wait_jobs()
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

function wsl_path_to_windows()
{
    local path="$1"
    if [ `echo "$path" | cut -c1-5` = "/mnt/" ]; then
        path="`echo "$path" | cut -c6`:`echo "$path" | cut -c7-`"
    fi
    path=${path////\\}
    echo $path
}
