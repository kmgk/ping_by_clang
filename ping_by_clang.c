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

	// ICMPパケットの設定
	const int PACKET_SIZE = 64; // ICMPヘッダ(8バイト) + データ(56バイト)
	char send_buffer[PACKET_SIZE];
	struct icmphdr *icmp_hdr = (struct icmphdr *)send_buffer;

	// ICMPヘッダの初期化
	memset(send_buffer, 0, PACKET_SIZE);
	icmp_hdr->icmp_type = ICMP_ECHO;
	icmp_hdr->icmp_code = 0;
	icmp_hdr->icmp_id = getpid(); // プロセスIDをIDとして使用
	icmp_hdr->icmp_seq = 1;				// シーケンス番号(今回は固定)

	// ペイロードにタイムスタンプを埋め込む（往復時間計測に必要）
	struct timeval *tval = (struct timeval *)(send_buffer + sizeof(struct icmphdr));
	gettimeofday(tval, NULL);

	// ICMPチェックサムの計算
	icmp_hdr->icmp_cksum = 0;
	icmp_hdr->icmp_cksum = in_cksum((unsigned short *)send_buffer, PACKET_SIZE);


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
