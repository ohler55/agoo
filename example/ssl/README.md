
Generate the self-signed cert.pem and key.pem files in this directory by running this command.

```
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -nodes -days 365 -subj '/CN=localhost'
```
