---
description: Run a multi-role multiflow workflow (plan -> implement -> review/test) on a task
argument-hint: "[recipe] <task description>"
---
Run an multiflow workflow for: $ARGUMENTS

Each workflow step runs as a separate headless agent session with its own role, model and
file permissions. You are only the launcher/monitor: do NOT implement the task yourself.

1. Call `multiflow_list_recipes`. If the arguments start with a recipe name, use it; otherwise pick the
   best-fitting recipe for the task (`quick_patch` for small well-understood changes, `bugfix` for
   defects, `feature` for new functionality) and say which one you picked and why.
2. Call `multiflow_start` with the recipe, the full task description and `backend: "claude"`.
   Pass `models` only if the user asked for specific models.
3. Report the run id and that progress can be followed with `multiflow status <run_id>`.
4. Poll `multiflow_status` (wait ~60s between polls using `sleep 60` in Bash) until the status is no longer
   `running`. Then summarize the history (each step's verdict and summary), read the final artifacts with
   `multiflow_read_artifact`, and show the user the resulting changes (`git diff --stat`).
   If the run halted, explain why from `status_reason` and suggest `multiflow resume --extra-attempt <run_id>`.
