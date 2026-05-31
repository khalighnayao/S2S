'Initializing git write access ...'

&git config --global credential.helper store
Add-Content "$HOME\.git-credentials" "https://$($env:git_access_token):x-oauth-basic@github.com`n"
&git config --global user.email "$($env:git_email)"
&git config --global user.name "$($env:git_user)"
# The GitHub distribution repo holds binary art/FPKs; never convert line endings.
&git config --global core.autocrlf false
