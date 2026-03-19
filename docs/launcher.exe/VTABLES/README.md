```bash
r2 ~/mxo/launcher.exe <<EOF | tee r2-vtables.txt
aaaa
av
y
EOF

ruby ../split.rb

rm r2-vtables.txt

rm $(grep -L 00438d80 *.md)


for x in *.md ; do echo "## VTable ${x/.md/}" ; echo '```' ; awk '{print $3$4}' <$x ; echo '```' ; echo ; done
```
