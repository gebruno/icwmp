# How to configure icwmp client for HTTPS connection to ACS

In order to keep the connection secure, most customers use HTTPS to connect to ACS.

In this case we need to have an ACS that supports HTTPS, And for that we use GenieACS as an example because it's an open source and supports HTTPS.

Find below the required steps to configure GenieACS server and icwmp client to support HTTPS

## Generating the private key and certificate

1. Generate a private key for the CA:

```bash
root@75f824228409:/opt/genieacs# openssl genrsa 2048 > ca-key.pem
Generating RSA private key, 2048 bit long modulus (2 primes)
...................................................................................+++++
...........................+++++
e is 65537 (0x010001)
```

2. Generate the X509 certificate for the CA:

```bash
root@75f824228409:/opt/genieacs# openssl req -new -x509 -nodes -days 365000 -key ca-key.pem -out ca-cert.pem
You are about to be asked to enter information that will be incorporated
into your certificate request.
What you are about to enter is what is called a Distinguished Name or a DN.
There are quite a few fields but you can leave some blank
For some fields there will be a default value,
If you enter '.', the field will be left blank.
-----
Country Name (2 letter code) [AU]:SE
State or Province Name (full name) [Some-State]:Stockholm
Locality Name (eg, city) []:Stockholm
Organization Name (eg, company) [Internet Widgits Pty Ltd]:IOPSYS
Organizational Unit Name (eg, section) []:IOPSYS
Common Name (e.g. server FQDN or YOUR name) []:genieacs
Email Address []:dev@iopsys.eu
root@75f824228409:/opt/genieacs# 
```

> Note: When generating the certificate, you must fill the Common Name filed with the correct URL server (for example here 'genieacs') otherwise you will get an error later when trying to connect to the ACS.

## Installing and Configuring GenieACS environment variables

1. Install GenieACS

You can follow the steps described in this [link](http://docs.genieacs.com/en/latest/installation-guide.html) in order to install GenieACS.

2. Configure GenieACS environment variables to support HTTPS

GenieACS offers a list of environment variables to configure the different features, you can see all the information in detail in this [link](http://docs.genieacs.com/en/latest/environment-variables.html).

In fact, the most important to us are these two variables below to configure the SSL functionality.

```bash
GENIEACS_CWMP_SSL_CERT=/path/to/certificate/file/ca-cert.pem
GENIEACS_CWMP_SSL_KEY=/path/to/certificate/key/file/ca-key.pem
```

## Checking the generated Certificate

You can use `openssl` command to check if there is any error in generated certificate.

```bash
openssl s_client -connect genieacs:7547 -CAfile ca-cert.pem
```

## Configuring DUT to support HTTPS

1. Copy the generated certificate under the needed path (default path is '/etc/ssl/certs/')

```bash
scp /path/of/certificate/ca-cert.pem root@192.168.1.1:/etc/ssl/certs
```

2. Set 'ssl_capath' option in cwmp config with certificate directory path (default path is '/etc/ssl/certs/')

```bash
uci set cwmp.acs.url='https://genieacs:7547'
uci set cwmp.acs.ssl_capath="/etc/ssl/certs"
ubus call uci commit '{"config":"cwmp"}'
```

Now, all required configuration are ready and you can start a cwmp connection using HTTPS.

