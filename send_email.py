# coding=utf-8

import smtplib, os, sys
from email.MIMEMultipart import MIMEMultipart
from email.MIMEText import MIMEText
from email.Utils import COMMASPACE, formatdate

def send_mail(server, username, password, send_from, send_to, subject, text):
    assert type(send_to)==list
    msg = MIMEMultipart()
    msg['From'] = send_from
    msg['To'] = COMMASPACE.join(send_to)
    msg['Date'] = formatdate(localtime=True)
    msg['Subject'] = subject

    msg.attach(MIMEText(text))

    smtp = smtplib.SMTP(server)
    #smtp.starttls()
    smtp.login(username, password)
    smtp.sendmail(send_from, send_to, msg.as_string())
    smtp.close()

send_mail(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], [sys.argv[5]], sys.argv[6], sys.argv[7])

