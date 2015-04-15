# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Documentation on PRESUBMIT.py can be found at:
# http://www.chromium.org/developers/how-tos/depottools/presubmit-scripts

EXCLUDE_PROJECT_CHECKS_DIRS = [ '.' ]

import subprocess
def CheckGitBranch():
  p = subprocess.Popen("git branch -vv", shell=True,
                       stdout=subprocess.PIPE)
  output, _ = p.communicate()

  lines = output.split('\n')
  for line in lines:
    # output format for checked-out branch should be
    # * branchname hash [TrackedBranchName ...
    toks = line.split()
    if '*' not in toks[0]:
      continue
    if not 'origin/master' in toks[3]:
      warning = 'Warning: your current branch:\n' + line
      warning += '\nis not tracking origin/master. git cl push may silently '
      warning += 'fail to push your change. To fix this, do\n'
      warning += 'git branch -u origin/master'
      return warning
    return None
  print 'Warning: presubmit check could not determine local git branch'
  return None

def _CommonChecks(input_api, output_api):
  """Checks for both upload and commit."""
  results = []
  results.extend(input_api.canned_checks.PanProjectChecks(
      input_api, output_api, project_name='Native Client',
      excluded_paths=tuple(EXCLUDE_PROJECT_CHECKS_DIRS)))
  branch_warning = CheckGitBranch()
  if branch_warning:
    results.append(output_api.PresubmitPromptWarning(branch_warning))
  return results

def CheckChangeOnUpload(input_api, output_api):
  """Verifies all changes in all files.
  Args:
    input_api: the limited set of input modules allowed in presubmit.
    output_api: the limited set of output modules allowed in presubmit.
  """
  report = []
  report.extend(_CommonChecks(input_api, output_api))
  return report

def CheckChangeOnCommit(input_api, output_api):
  """Verifies all changes in all files and verifies that the
  tree is open and can accept a commit.
  Args:
    input_api: the limited set of input modules allowed in presubmit.
    output_api: the limited set of output modules allowed in presubmit.
  """
  report = []
  report.extend(CheckChangeOnUpload(input_api, output_api))
  return report

def GetPreferredTrySlaves(project, change):
  return []
