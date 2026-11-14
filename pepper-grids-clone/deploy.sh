   #!/bin/sh
   set -e
   cd "$(dirname "$0")"
   rsync -avz --delete ./project/ root@bela.local:/root/Bela/projects/pepper-grids-clone/
   