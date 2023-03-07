package memory

func Slice(s []byte, offset, length int) []byte {
	return s[offset : offset+length]
}
