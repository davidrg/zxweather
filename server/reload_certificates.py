"""
A program for triggering a reload of the zxweather servers SSL certificates
online without having to restart the daemon. Useful if you're using something
like certbot to replace the certificates rapidly.

To do this you need to set an SSL reload password in the servers configuration
file and use that password when running this program.

Example:
    reload_certificates.py wss://localhost:443/ my-hard-to-guess-reload-password

If the servers existing certificates have already expired you can supply the
--no-ssl-validation option to skip validating the servers current certificate.
"""
import ssl
from argparse import ArgumentParser

from websocket import create_connection


def reload_certificate(server, password, disable_ssl_validation):
    if disable_ssl_validation:
        ws = create_connection(server, sslopt={"cert_reqs": ssl.CERT_NONE})
    else:
        ws = create_connection(server)

    print("Requesting SSL Certificate reload...")
    ws.send("ssl_reload {0}".format(password))
    print("Waiting for response...")
    result = ws.recv()
    print("Received '%s'" % result)
    ws.close()

    if result.startswith("ERROR"):
        print("Certificate reload failed.")
        exit(1)


def main():
    parser = ArgumentParser(
        description="Triggers reloading of SSL certificates for the zxweather "
                    "websocket server")

    parser.add_argument("websocket_uri",
                        type=str,
                        help="URI for the websocket server to connect to. For "
                             "example, wss://localhost:443/")
    parser.add_argument("password",
                        type=str,
                        help="Certificate reload password (this comes from the "
                             "servers configuration file)")
    parser.add_argument("--no-ssl-validation",
                        dest="no_ssl_validation",
                        action="store_true",
                        default=False,
                        help="Don't validate SSL certificates")

    args = parser.parse_args()

    uri = args.websocket_uri
    password = args.password
    disable_ssl_validation = args.no_ssl_validation

    reload_certificate(uri, password, disable_ssl_validation)


if __name__ == "__main__":
    main()
