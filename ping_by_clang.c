#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h> // for gettimeofday

// チェックサム計算のための関数プロトタイプ
unsigned short in_cksum(unsigned short *addr, int len);

int main(int argc, char *argv[])
{
	// ターゲットホスト名またはIPアドレスのチェック
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <hostname_or_IP_address>\n", argv[0]);
		return 1;
	}

	char *target_host = argv[1];
	int sockfd;
	struct sockaddr_in dest_addr;

	// ソケットの作成
	// AF_INET: IPv4
	// SOCK_RAW: RAWソケット
	// IPPROTO_ICMP: ICMPプロトコル
	if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
	{
		perror("socket error");
		return 1;
	}

	// 宛先アドレスの設定
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;

	// ホスト名解決またはIPアドレス変換
	if (inet_pton(AF_INET, target_host, &(dest_addr.sin_addr)) <= 0)
	{
		// IPアドレスとして認識できなかった場合、ホスト名解決を試みる
		struct hostent *he;
		if ((he = gethostbyname(target_host)) == NULL)
		{
			perror("gethostbyname error");
			close(sockfd);
			return 1;
		}
		memcpy(&(dest_addr.sin_addr), he->h_addr, he->h_length);
	}

	printf("PING %s (%s)\n", target_host, inet_ntoa(dest_addr.sin_addr));

	// ICMPパケットの設定
	const int PACKET_SIZE = 64; // ICMPヘッダ(8バイト) + データ(56バイト)
	char send_buffer[PACKET_SIZE];
	struct icmphdr *icmp_hdr = (struct icmphdr *)send_buffer;

	// ICMPヘッダの初期化
	memset(send_buffer, 0, PACKET_SIZE);
	icmp_hdr->type = ICMP_ECHO;
	icmp_hdr->code = 0;
	icmp_hdr->un.echo.id = getpid(); // プロセスIDをIDとして使用
	icmp_hdr->un.echo.sequence = 1;				// シーケンス番号(今回は固定)

	// ペイロードにタイムスタンプを埋め込む（往復時間計測に必要）
	struct timeval *tval = (struct timeval *)(send_buffer + sizeof(struct icmphdr));
	gettimeofday(tval, NULL);

	// ICMPチェックサムの計算
	icmp_hdr->checksum = 0;
	icmp_hdr->checksum = in_cksum((unsigned short *)send_buffer, PACKET_SIZE);

	printf("PING %s (%s) %d(%d) bytes of data.\n", target_host, inet_ntoa(dest_addr.sin_addr), PACKET_SIZE - sizeof(struct icmphdr), PACKET_SIZE);

	int count = 0;
	const int MAX_PING = 4; // 送信するPingの回数

	for (count = 0; count < MAX_PING; count++)
	{
		// シーケンス番号を更新
		icmp_hdr->un.echo.sequence = htons(count);
		// ペイロードにタイムスタンプを再度埋め込み
		gettimeofday(tval, NULL);

		// チェックサムを再計算
		icmp_hdr->checksum = 0;
		icmp_hdr->checksum = in_cksum((unsigned short *)send_buffer, PACKET_SIZE);

		// パケットの送信
		if (sendto(sockfd, send_buffer, PACKET_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) <= 0)
		{
			perror("sendto error");
			break;
		}

		// 応答の受信
		char recv_buffer[1500]; // 最大MTUサイズを考慮
		struct sockaddr_in from_addr;
		socklen_t from_len = sizeof(from_addr);
		int bytes_received = recvfrom(sockfd, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr *)&from_addr, &from_len);
		if (bytes_received < 0) {
			perror("recvfrom error");
			continue;
		}

		// IPヘッダとICMPヘッダの解析
		struct ip *ip_hdr = (struct ip *)recv_buffer;
		int ip_hdr_len = ip_hdr->ip_hl * 4; // IPヘッダ長は4バイト単位

		struct icmphdr *recv_icmp_hdr = (struct icmphdr *)(recv_buffer + ip_hdr_len);

		// 受信したパケットがICMPエコー応答であり、かつ自分のID/シーケンス番号と一致するか確認
		if (recv_icmp_hdr->type == ICMP_ECHOREPLY &&
			recv_icmp_hdr->un.echo.id == getpid() &&
			recv_icmp_hdr->un.echo.sequence == htons(count))
		{
			// 往復時間の計算
			struct timeval *recv_tval = (struct timeval *)(recv_buffer + ip_hdr_len + sizeof(struct icmphdr));
			struct timeval tv_now;
			gettimeofday(&tv_now, NULL);

			long long rtt_us = (tv_now.tv_sec - recv_tval->tv_sec) * 1000000LL + (tv_now.tv_usec - recv_tval->tv_usec);
			double rtt_ms = (double)rtt_us / 1000.0;

			printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.2f ms\n",
				bytes_received - ip_hdr_len,
				inet_ntoa(from_addr.sin_addr),
				ntohs(recv_icmp_hdr->un.echo.sequence),
				ip_hdr->ip_ttl,
				rtt_ms);
		}
		else
		{
			printf("Received unexpected ICMP packet type %d or ID/Seq mismatch\n", recv_icmp_hdr->type);
		}

		sleep(1);
	}


	close(sockfd); // close socket

	return 0;
}

// ICMPチェックサム計算関数
unsigned short in_cksum(unsigned short *addr, int len) {
	int sum = 0;
	unsigned short answer = 0;
	unsigned short *w = addr;
	int nleft = len;

	// 16ビットずつ加算
	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}

	// 奇数バイトが残っていた場合
	if (nleft == 1)
	{
		*(unsigned char *)(&answer) = *(unsigned char *)w;
		sum += answer;
	}

	// キャリーを畳み込む
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	// 1の補数を取る
	answer = ~sum;
	return answer;
}
