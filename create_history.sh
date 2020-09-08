#!/bin/sh

hg log -G | awk -F'  +' -v CBR="`hg branch`" '
	BEGIN              {printf "\"Branch: %s\\n\\n\"\n",CBR }
	($2=="changeset:") {G=$1;OUT=$3;sub(/:.*/,"",OUT)}
	($2=="branch:")    {BR=$3}
	($2=="date:")      {OUT=OUT" "$3}
	($2=="summary:")   {OUT=OUT" "$3}
	($2=="")           {gsub(/\"/,"",OUT);printf "\"%-6s %-10s %s\\n\"\n",G,BR,OUT; OUT=BR=""}
' | head -n 30  >./fs/proc/history.inc

