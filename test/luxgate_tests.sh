 #!/bin/bash

find test/functional -name "luxgate_*" -printf "\nRunning %f\n\n" -exec "./{}" \;
