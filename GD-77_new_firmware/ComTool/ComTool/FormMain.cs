﻿/*
 * Copyright (C)2019 Kai Ludwig, DG4KLU
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. The name of the author may not be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

using System;
using System.ComponentModel;
using System.Drawing;
using System.IO.Ports;
using System.Threading;
using System.Windows.Forms;

namespace ComTool
{
    public partial class FormMain : Form
    {
        bool running = false;
        BackgroundWorker worker;
        bool stop_worker = false;
        bool form_close = false;

        SerialPort port = null;
        
        public FormMain()
        {
            InitializeComponent();
        }

        private void FormMain_Load(object sender, EventArgs e)
        {
            this.Location = new Point((Screen.PrimaryScreen.WorkingArea.Width - this.Width) / 2, (Screen.PrimaryScreen.WorkingArea.Height - this.Height) / 2);

            loadCOMPortlist();
        }

        private void FormMain_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (running)
            {
                stop_worker = true;
                form_close = true;
                e.Cancel = true;
            }
        }

        void loadCOMPortlist()
        {
            string old_item = comboBoxCOMPorts.Text;
            comboBoxCOMPorts.Items.Clear();
            string[] ports = SerialPort.GetPortNames();
            foreach (string port in ports)
            {
                comboBoxCOMPorts.Items.Add(port);
            }
            if (comboBoxCOMPorts.Items.Contains(old_item))
            {
                comboBoxCOMPorts.Text = old_item;
            }
        }

        void UpdateStatus(string text)
        {
            if (labelStatusText.InvokeRequired)
                labelStatusText.Invoke(new MethodInvoker(delegate ()
                {
                    labelStatusText.Text = text;
                }));
            else
            {
                labelStatusText.Text = text;
            }
        }

        private void ClearLog()
        {
            richTextBoxLog.BeginInvoke(new Action(() =>
            {
                richTextBoxLog.Text = "";
            }));
        }

        private void SetLog(String text)
        {
            richTextBoxLog.BeginInvoke(new Action(() =>
            {
                if (richTextBoxLog.Text != "")
                {
                    richTextBoxLog.AppendText("\r\n");
                }
                richTextBoxLog.AppendText(text);
                if (richTextBoxLog.Text.Length > 32000)
                {
                    richTextBoxLog.Text = richTextBoxLog.Text.Substring(richTextBoxLog.Text.Length - 32000);
                }
                richTextBoxLog.SelectionStart = richTextBoxLog.Text.Length;
                richTextBoxLog.ScrollToCaret();
            }));
        }

        void worker_DoWork(object sender, DoWorkEventArgs e)
        {
            byte[] sendbuffer = new byte[512];
            byte[] readbuffer = new byte[512];

            byte[] com_Buf = new byte[256];
            int com_Buf_pos = 0;

            stop_worker = false;

            ClearLog();
            SetLog("START");

            while (!stop_worker)
            {
                try
                {
                    sendbuffer[0] = (byte)'B';
                    port.Write(sendbuffer, 0, 1);
                    port.Read(readbuffer, 0, 64);

                    if (readbuffer[0] == 'B')
                    {
                        int len = (readbuffer[1] << 8) + (readbuffer[2] << 0);
                        for (int i = 0; i < len; i++)
                        {
                            if (com_Buf_pos < com_Buf.Length)
                            {
                                com_Buf[com_Buf_pos] = readbuffer[i+3];
                                com_Buf_pos++;

                                if ((com_Buf[0] == 0x42) && (com_Buf_pos == 1 + 4))
                                {
                                    string line = "0x42:";
                                    for (int j = 0; j < 4; j++)
                                    {
                                        line = line + String.Format(" {0:X2}", com_Buf[1 + j]);
                                    }
                                    SetLog(line);
                                    com_Buf_pos = 0;
                                }
                                else if ((com_Buf[0] == 0x84) && (com_Buf_pos == 1 + 8))
                                {
                                    string line = "0x84:";
                                    for (int j = 0; j < 8; j++)
                                    {
                                        line = line + String.Format(" {0:X2}", com_Buf[1 + j]);
                                    }
                                    SetLog(line);
                                    com_Buf_pos = 0;
                                }
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message);
                    break;
                }

                Thread.Sleep(10);
            }

            SetLog("STOP");
        }

        void worker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            port.Close();

            buttonStartStop.Text = "Start";
            running = false;

            if (form_close)
            {
                Close();
            }
        }

        private void buttonStartStop_Click(object sender, EventArgs e)
        {
            if (!running)
            {
                try
                {
                    port = new SerialPort(comboBoxCOMPorts.Text, 115200, Parity.None, 8, StopBits.One);
                    port.ReadTimeout = 1000;
                    port.Open();

                    worker = new BackgroundWorker();
                    worker.DoWork += new DoWorkEventHandler(worker_DoWork);
                    worker.RunWorkerCompleted += new RunWorkerCompletedEventHandler(worker_RunWorkerCompleted);
                    worker.RunWorkerAsync();

                    buttonStartStop.Text = "Stop";
                    running = true;
                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message);
                }
            }
            else
            {
                stop_worker = true;
            }
        }

        private void buttonRefreshCOMPortlist_Click(object sender, EventArgs e)
        {
            loadCOMPortlist();
        }
    }
}