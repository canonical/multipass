---
agent: agent
description: Run a multi-role multiflow workflow (plan -> implement -> review/test) on a task
argument-hint: "[quick_patch|bugfix|feature|pr_split] <task description>"
---
Run a multiflow workflow for: ${input:task:[quick_patch|bugfix|feature|pr_split] task description}

Each workflow step runs as a separate headless agent session with its own role, model and
file permissions. You are only the launcher/monitor: do NOT implement the task yourself.

1. Call the `multiflow_list_recipes` tool. If the input starts with a recipe name, use it; otherwise pick the
   best-fitting recipe (`quick_patch` for small well-understood changes, `bugfix` for defects, `feature` for
   new functionality, `pr_split` to split a large PR into stacked branches) and say which one you picked and why.
2. Call `multiflow_start` with the recipe and the full task description. Omit `backend` unless the user asked
   for one: it defaults to the harness you are running in.
   Pass `models` only if the user asked for specific models.
3. Report the run id and that progress can be followed with `multiflow status <run_id>` in a terminal.
4. Poll `multiflow_status` (run `sleep 60` in the terminal between polls) until the status is no longer
   `running`. Then summarize each step's verdict and summary, read the final artifacts with
   `multiflow_read_artifact`, and show the resulting changes (`git diff --stat`).
   If the run halted, explain why from `status_reason` and suggest `multiflow resume --extra-attempt <run_id>`.
