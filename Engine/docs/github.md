# 🏄‍♀️ GitHub Workflow Guide

This guide provides a comprehensive, step-by-step procedure to develop a new feature or fix a bug in our repository, from the initial creation to the final merge into the main branch. This workflow encourages trunk-based development and ensures that the branches remain updated with the main branch.

## Creating a Development Branch

Start by updating the `main` branch before creating a new development branch:
```bash
git checkout main
git pull
```

Then create a new branch based on the current state of `main`. This step creates an isolated environment for your changes without affecting the main branch.
```bash
git checkout -b my-branch-name
``` 

Replace `my-branch-name` with a suitable name for your feature or fix.

> **Note** 
> Naming convention for branches: `my-branch-name` 

## Developing Your Feature or Fix

Begin coding on your branch. It's important to commit your changes regularly to save your work and keep track of your progress. 

Use `git status` to get an overview of your working directory. This command displays the state of your current branch, any changes you've made, and files that are staged for the next commit.

Once you've made some changes and are ready to commit, use `git add .` to stage all the changes, or `git add filename` to stage specific files.

Use the following command to commit your changes: 
```bash 
git commit -m "commit message"
```

> **Note** 
> Your commit messages should be clear, descriptive, and begin with an emoji for quick context.

Once you've made your changes and committed them, you'll need to push these changes to your remote branch using the `git push` command.


## Keeping Your Branch Up-to-date

To keep your branch up-to-date with the `main` branch, you should regularly rebase your branch. This process takes the changes added to `main` and applies them to your branch, rewriting the branch history so that your changes are **after** the ones from `main`. [^1] 

Ensure all changes in your local branch are [committed](#developing-your-feature-or-fix), then update `main`:
```bash
git checkout main
git pull
```

Then, switch to your branch and 
```bash
git checkout my-branch-name
git pull
``` 

Next, rebase your branch: 
```bash
git rebase main -i
``` 

The `-i` flag starts an interactive rebase, enabling you to edit, reword, drop, or combine commits for a cleaner history.

Lastly, force push to update your remote branch with the rebased changes:
```bash
git push --force
``` 

> **Warning**
> If you forget to use `--force`, you will see the rebased history of `main` (commits since you last rebased) in your branch. To rectify this, perform a new rebase and `git push --force`.

## Submitting a Pull Request

Once you've completed your feature or fix, and have kept your branch updated with the `main` branch, you can submit a pull request (PR). The `main` branch is locked from direct pushes, making PRs necessary.

Navigate to your repository on GitHub, select `Pull Requests`, then `New Pull Request`. Choose the branch to compare with `main` and provide the relevant details. 

Your PR title should begin with an emoji for immediate context. This title (including the emoji) will be part of the commit history when the PR is merged into `main`.

> **Note** 
> Naming convention for PR titles: `📃 My PR title` 

## Review, Passing GitHub Actions, and Merge

The final step is the review process. The

 team will review your PR and may request changes. If changes are needed, update your branch and rebase `main` on your branch before requesting a new review. 

Ensure your PR passes all GitHub Actions checks, which include automated tests, linting, etc. PRs that don't pass these checks cannot be merged into `main`.

After your PR has been reviewed, any necessary changes made, and all GitHub Actions checks passed, perform a final rebase of your branch to the latest version of `main` and run a final test on your changes. 

Finally, on the PR webpage, click on the "Merge" button. This merges your branch into `main`, thus concluding your development workflow.


## References
[^1]: More info on [rebasing](https://git-scm.com/book/en/v2/Git-Branching-Rebasing)