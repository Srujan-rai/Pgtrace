# Contributing

Thanks for your interest in Pgtrace! Contributions are welcome.

## Development Setup

```bash
make clean
make
sudo make install
sudo systemctl restart postgresql@16-main
```

## How to Contribute

1. Fork the repository and create a feature branch.
2. Make focused changes with clear commit messages.
3. If you add or change SQL functions, update the extension SQL file.
4. Add or update documentation as needed.
5. Open a pull request with a clear description of the change.

## Reporting Issues

- Include PostgreSQL version and OS details.
- Include exact error messages and logs.
- Provide steps to reproduce if possible.

## Code Style

- Keep changes minimal and consistent with existing style.
- Avoid unrelated refactors in the same PR.
