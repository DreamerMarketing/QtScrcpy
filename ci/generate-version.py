import sys
import os

if __name__ == '__main__':
    p = os.popen('git rev-list --tags --max-count=1')
    commit = p.read().strip()
    p.close()
    if not commit:
        sys.exit(0)

    p = os.popen('git describe --tags ' + commit)
    tag = p.read().strip()
    p.close()
    if not tag:
        sys.exit(0)

    # print('get tag:', tag)

    version = str(tag[1:] if tag.startswith('v') else tag)
    version_file = os.path.abspath(os.path.join(os.path.dirname(__file__), "../QtScrcpy/appversion"))
    file=open(version_file, 'w')
    file.write(version)
    file.close()
    sys.exit(0)
