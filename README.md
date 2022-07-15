# svn-shell

## Introduction 

svn-shell like [git-shell](https://git-scm.com/docs/git-shell) work with ssh.

## Install
```sh
git clone https://github.com/QCute/svn-shell
cd svn-shell
make
make test
make install
```

## Configution
* the [~/svn-shell-commands]() directory like [~/git-shell-commands]()  
* the [~/svn-shell-commands/no-interactive-login] file like [~/git-shell-commands/no-interactive-login]()  
* default serve root directory is [~]() (The HOME environment value)  
* other svnserve options get from [/etc/default/svnserve]() config  

[/etc/default/svnserve]() example
```ini
# tunnel username
--tunnel-user=svn
```

## License

svn-shell is licensed under the [GNU General Public License v3 (GPL-3)](http://www.gnu.org/copyleft/gpl.html).
