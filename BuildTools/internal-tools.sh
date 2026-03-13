#!/bin/bash -e

declare -a JOBS

function has_arg()
{
    local arg
    for arg in $PROGRAM_ARGS; do
        for expected in "$@"; do
            if [[ $arg = $expected ]]; then
                return 0
            fi
        done
    done
    return 1
}

function run_job()
{
    eval "$1" & JOBS[$!]="$1"
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

function verify_workspace_part()
{
    if [[ ! -f "$1-version.txt" || $(cat "$1-version.txt") != $2 ]]; then
        if has_arg check; then
            echo "Workspace is not ready"
            exit 1
        fi

        workspace_job()
        {
            eval $3
            cd $FO_WORKSPACE
            echo $2 > "$1-version.txt"
        }

        run_job "workspace_job $1 $2 $3"
    fi
}

PROGRAM_ARGS=$@

function check_arg()
{
    if has_arg "$@"; then
		echo "1"
	fi
}
