import subprocess
import sys

if __name__ == '__main__':
  namespace_to_replace = sys.argv[1]
  output = subprocess.check_output([
    'git', 'grep', '-l', 'namespace %s {' % namespace_to_replace])
  files = [l for l in output.splitlines()]

  for path in files:
    lines = []
    with open(path) as f:
      lines = f.readlines()
    output_lines = []
    for l in lines:
      if l.startswith('namespace %s {' % namespace_to_replace):
        output_lines.append('// TODO(bugs.webrtc.org/7484): Remove after namespace cleanup is done.\n')
        output_lines.append('#include "rtc_base/cricket_namespace_compatibility.h"\n')
        output_lines.append('\n')
        output_lines.append('namespace webrtc {\n')
      elif l.startswith('}  // namespace %s' % namespace_to_replace):
        output_lines.append('}  // namespace webrtc\n')
      else:
        output_lines.append(l)

    with open(path, 'w') as f:
      for l in output_lines:
        f.write(l)
