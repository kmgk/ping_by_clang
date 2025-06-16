#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
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
	if ((sockfd = socket(AF_INET, SOCK_RAW, IPPPROTO_ICMP)) < 0)
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
		if ((he = gethostname(target_host)) == NULL)
		{
			perror("gethostbyname error");
			close(sockfd);
			return 1;
		}
		memcpy(&(dest_addr.sin_addr), he->h_addr, he->h_length);
	}

	printf("PING %s (%s)\n", target_host, inet_ntoa(dest_addr.sin_addr));

	// ここにパケット送受信のロジックを記述

	close(sockfd); // close socket

	return 0;
}

// ICMPチェックサム計算関数
unsigned short in_cksum(unsigned short *addr, int len) {
	// 後で実装
	return 0;
}
